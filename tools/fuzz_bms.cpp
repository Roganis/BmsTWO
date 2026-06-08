// Headless harness to exercise Bms::BmsReader against pathological charts
// under AddressSanitizer/UBSan. Not part of the app build; compiled ad-hoc
// against the instrumented object files. See tools/run_fuzz_bms.sh.
#include "bms/Bms.h"
#include "MainWindow.h"
#include <QApplication>
#include <QSettings>
#include <QTextStream>
#include <QFile>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QDir>
#include <cstdio>

static void writeFile(const QString &path, const QString &content) {
    QFile f(path);
    f.open(QFile::WriteOnly | QFile::Text);
    QTextStream(&f) << content;
}

static void run(const QString &name, const QString &path) {
    fprintf(stderr, "=== case: %s ===\n", qPrintable(name));
    Bms::BmsReader reader(path);
    QVariant arg;
    int guard = 0;
    while (++guard < 5000000) {
        Bms::BmsReader::Status st = reader.Load(arg);
        if (st == Bms::BmsReader::STATUS_COMPLETE) { fprintf(stderr, "  complete\n"); return; }
        if (st == Bms::BmsReader::STATUS_ERROR)    { fprintf(stderr, "  error\n");    return; }
        arg = (st == Bms::BmsReader::STATUS_ASK) ? reader.GetDefaultValue() : QVariant();
    }
    fprintf(stderr, "  hit iteration guard\n");
}

int main(int argc, char **argv) {
    QCoreApplication::setApplicationName("BmsTWOFuzz");
    // Pre-seed the INI file App() will read, with a matching ConfigVersion so
    // App does not clear it, and BmsReader settings that force non-interactive
    // parsing via the inline #RANDOM path (the stack-recursion surface).
    {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QString ini = dir.isEmpty() ? QString() : QDir(dir).filePath("Settings.ini");
        QSettings *s = ini.isEmpty() ? new QSettings("ExclusionBms", "BmsONE")
                                     : new QSettings(ini, QSettings::IniFormat);
        s->setValue("ConfigVersion", 1);
        s->setValue("BmsReader/AskTextEncoding", false);
        s->setValue("BmsReader/AskRandomValues", false);
        s->setValue("BmsReader/AskGameMode", false);
        s->setValue("BmsReader/UseRandomValues", true);
        s->sync();
        delete s;
    }
    App app(argc, argv);

    QTemporaryDir dir;
    const QString d = dir.path() + "/";

    // A: many consecutive #RANDOM (recursion depth via inline cont->LoadMain)
    {
        QString c;
        for (int i = 0; i < 200000; i++) c += "#RANDOM 2\n";
        writeFile(d + "a.bms", c);
        run("many-random", d + "a.bms");
    }
    // B: deeply nested #RANDOM/#IF, never closed
    {
        QString c;
        for (int i = 0; i < 100000; i++) c += "#RANDOM 2\n#IF 1\n";
        writeFile(d + "b.bms", c);
        run("nested-if-unclosed", d + "b.bms");
    }
    // C: control-flow underflow: stray #ENDIF / #ELSE / #ELSEIF / #ENDRANDOM
    {
        QString c = "#ENDIF\n#ELSE\n#ELSEIF 1\n#ENDRANDOM\n#IF 1\n#ENDRANDOM\n";
        writeFile(d + "c.bms", c);
        run("control-underflow", d + "c.bms");
    }
    // D: extreme section numbers + huge-resolution channel lines
    {
        QString c = "#PLAYER 1\n#BPM 120\n";
        QString longobj; for (int i = 0; i < 4000; i++) longobj += "01";
        c += "#999110:" + longobj + "\n";
        c += "#00011:" + longobj + "\n";
        c += "#00011:0102\n"; // forces LCM resolution blow-up vs the long line
        writeFile(d + "d.bms", c);
        run("extreme-sections", d + "d.bms");
    }
    // E: malformed garbage lines
    {
        QString c = "#\n#:\n#123\n#00011:0\n#00011:123\n#WAV\n#RANDOM\n#RANDOM -5\n#RANDOM abc\n#IF\n#SETRANDOM 0\n";
        writeFile(d + "e.bms", c);
        run("malformed", d + "e.bms");
    }
    // F: #RANDOM with value 0 then #IF 0
    {
        QString c = "#RANDOM 0\n#IF 0\n#00011:0101\n#ENDIF\n#ENDRANDOM\n";
        writeFile(d + "f.bms", c);
        run("random-zero", d + "f.bms");
    }
    fprintf(stderr, "ALL CASES DONE\n");
    return 0;
}
