#include "ScanLine.h"

void Painter::Move(Pointf p)
{
	Segment& m = PathAdd<Segment>();
	m.kind = MOVE;
	m.p = p;
}

void Painter::Line(Pointf p)
{
	Segment& m = PathAdd<Segment>();
	m.kind = LINE;
	m.p = p;
}

void Painter::Quadratic(Pointf p1, Pointf p)
{
	QuadraticSegment& m = PathAdd<QuadraticSegment>();
	m.kind = QUADRATIC;
	m.p = p;
	m.p1 = p1;
}

void Painter::Cubic(Pointf p1, Pointf p2, Pointf p)
{
	CubicSegment& m = PathAdd<CubicSegment>();
	m.kind = QUADRATIC;
	m.p = p;
	m.p1 = p1;
	m.p2 = p2;
}

