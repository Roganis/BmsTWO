#ifndef RESOLUTIONUTIL_H
#define RESOLUTIONUTIL_H

#include <QSet>
#include <QJsonArray>


class ResolutionUtil
{
	static int gcd(int M, int n){
		int l = M % n;
		return l == 0 ? n : gcd(n, l);
	}
public:
	static int ConvertTicks(int original, int newResolution, int oldResolution)
	{
		if (oldResolution <= 0) return original;
		return original * newResolution / oldResolution;
	}
	static int GetAcceptableDivider(QSet<int> locs){
		// GCD of all note locations. A location of 0 lies on every grid, so it
		// must be skipped; otherwise `n` could become 0 and the modulo in gcd()
		// would divide by zero (QSet iteration order is unspecified, so a 0 may
		// be the first element). Returns 0 when there are no positive locations.
		int n = 0;
		for (int loc : locs){
			if (loc <= 0)
				continue;
			n = (n == 0) ? loc : gcd(n, loc);
		}
		return n;
	}
};


#endif // RESOLUTIONUTIL_H
