#include "Draw.h"

NAMESPACE_UPP

#ifdef PLATFORM_X11

#define LLOG(x)     // LOG(x)
#define LTIMING(x)  // TIMING(x)

void SystemDraw::BeginOp()
{
	Cloff f = cloff.Top();
	Vector<Rect> newclip;
	newclip <<= clip.Top();
	f.clipi = clip.GetCount();
	clip.Add() = newclip;
	cloff.Add(f);
}

void SystemDraw::OffsetOp(Point p)
{
	Cloff f = cloff.Top();
	f.offseti = offset.GetCount();
	actual_offset += p;
	offset.Add(actual_offset);
	cloff.Add(f);
}

bool SystemDraw::ClipOp(const Rect& r)
{
	LLOG("SystemDraw::ClipOp(" << r << ")");
	Cloff f = cloff.Top();
	bool ch = false;
	Vector<Rect> newclip = Intersect(clip.Top(), r + actual_offset, ch);
	if(ch) {
		f.clipi = clip.GetCount();
		clip.Add() = newclip;
	}
	cloff.Add(f);
	if(ch)
		SetClip();
	return clip.Top().GetCount();
}

bool SystemDraw::ClipoffOp(const Rect& r)
{
	LLOG("SystemDraw::ClipOffOp(" << r << ")");
	Cloff f = cloff.Top();
	bool ch = false;
	Vector<Rect> newclip = Intersect(clip.Top(), r + actual_offset, ch);
	if(ch) {
		f.clipi = clip.GetCount();
		clip.Add() = newclip;
	}
	f.offseti = offset.GetCount();
	actual_offset += r.TopLeft();
	offset.Add(actual_offset);
	cloff.Add(f);
	if(ch)
		SetClip();
	return clip.Top().GetCount();
}

void SystemDraw::EndOp()
{
	ASSERT(cloff.GetCount());
	cloff.Drop();
	actual_offset = offset[cloff.Top().offseti];
	clip.SetCount(cloff.Top().clipi + 1);
	SetClip();
}

bool SystemDraw::ExcludeClipOp(const Rect& r)
{
	LLOG("SystemDraw::ExcludeClipOp(" << r << ")");
	CloneClip();
	Vector<Rect>& cl = clip.Top();
	bool ch = false;
	Vector<Rect> ncl = Subtract(cl, r + actual_offset, ch);
	if(ch) {
		cl = ncl;
		SetClip();
	}
	return clip.Top().GetCount();
}

bool SystemDraw::IntersectClipOp(const Rect& r)
{
	CloneClip();
	Vector<Rect>& cl = clip.Top();
	bool ch = false;
	Vector<Rect> ncl = Intersect(cl, r + actual_offset, ch);
	if(ch) {
		cl = ncl;
		SetClip();
	}
	return clip.Top().GetCount();
}

bool SystemDraw::IsPaintingOp(const Rect& r) const
{
	LTIMING("IsPaintingOp");
	Rect rr = r + actual_offset;
	const Vector<Rect>& cl = clip[cloff.Top().clipi];
	for(int i = 0; i < cl.GetCount(); i++)
		if(cl[i].Intersects(rr))
			return true;
	return false;
}

void SystemDraw::DrawRectOp(int x, int y, int cx, int cy, Color color)
{
	LTIMING("DrawRect");
	DrawLock __;
	LLOG("DrawRect " << RectC(x, y, cx, cy) << ": " << color);
	if(IsNull(color)) return;
	if(cx <= 0 || cy <= 0) return;
	if(color == InvertColor) {
		XSetFunction(Xdisplay, gc, GXinvert);
		XFillRectangle(Xdisplay, dw, gc, x + actual_offset.x, y + actual_offset.y, cx, cy);
		XSetFunction(Xdisplay, gc, GXcopy);
	}
	else {
		SetForeground(color);
		XFillRectangle(Xdisplay, dw, gc, x + actual_offset.x, y + actual_offset.y, cx, cy);
	}
}

void SystemDraw::DrawLineOp(int x1, int y1, int x2, int y2, int width, Color color)
{
	DrawLock __;
	if(IsNull(width) || IsNull(color)) return;
	SetLineStyle(width);
	SetForeground(color);
	XDrawLine(Xdisplay, dw, gc,
	          x1 + actual_offset.x, y1 + actual_offset.y,
	          x2 + actual_offset.x, y2 + actual_offset.y);
}

void SystemDraw::DrawPolyPolylineOp(const Point *vertices, int vertex_count,
	                          const int *counts, int count_count,
	                          int width, Color color, Color doxor)
{
	DrawLock __;
	ASSERT(count_count > 0 && vertex_count > 0);
	if(vertex_count < 2 || IsNull(color))
		return;
	XGCValues gcv_old, gcv_new;
	XGetGCValues(Xdisplay, GetGC(), GCForeground | GCLineWidth | GCFunction, &gcv_old);
	gcv_new.function = IsNull(doxor) ? X11_ROP2_COPY : X11_ROP2_XOR;
	gcv_new.foreground = GetXPixel(color) ^ (IsNull(doxor) ? 0 : GetXPixel(doxor));
	gcv_new.line_width = width;
	XChangeGC(Xdisplay, GetGC(), GCForeground | GCLineWidth | GCFunction, &gcv_new);
	enum { REQUEST_LENGTH = 256 }; // X server XDrawLines request length (heuristic)
	Point offset = GetOffset();
	if(vertex_count == 2)
		XDrawLine(Xdisplay, GetDrawable(), GetGC(),
			vertices[0].x + offset.x, vertices[0].y + offset.y,
			vertices[1].x + offset.x, vertices[1].y + offset.y);
	else if(count_count == 1 || vertex_count > count_count * (REQUEST_LENGTH + 2)) {
		for(; --count_count >= 0; counts++)
		{
			Buffer<XPoint> part(*counts);
			for(XPoint *vo = part, *ve = vo + *counts; vo < ve; vo++, vertices++)
			{
				vo -> x = (short)(vertices -> x + offset.x);
				vo -> y = (short)(vertices -> y + offset.y);
			}
			XDrawLines(Xdisplay, GetDrawable(), GetGC(), part, *counts, CoordModeOrigin);
		}
	}
	else {
		int segment_count = vertex_count - count_count;
		Buffer<XSegment> segments(segment_count);
		XSegment *so = segments;
		while(--count_count >= 0)
		{
			const Point *end = vertices + *counts++;
			so -> x1 = (short)(vertices -> x + offset.x);
			so -> y1 = (short)(vertices -> y + offset.y);
			vertices++;
			so -> x2 = (short)(vertices -> x + offset.x);
			so -> y2 = (short)(vertices -> y + offset.y);
			so++;
			while(++vertices < end) {
				so -> x1 = so[-1].x2;
				so -> y1 = so[-1].y2;
				so -> x2 = (short)(vertices -> x + offset.x);
				so -> y2 = (short)(vertices -> y + offset.y);
				so++;
			}
		}
		ASSERT(so == segments + segment_count);
		XDrawSegments(Xdisplay, GetDrawable(), GetGC(), segments, segment_count);
	}
	XChangeGC(Xdisplay, GetGC(), GCForeground | GCLineWidth | GCFunction, &gcv_old);
}

static void DrawPolyPolyPolygonRaw(SystemDraw& draw, const Point *vertices, int vertex_count,
	const int *subpolygon_counts, int subpolygon_count_count, const int *, int)
{
	DrawLock __;
	Point offset = draw.GetOffset();
	const Point *in = vertices;
	for(int i = 0; i < subpolygon_count_count; i++) {
		int n = subpolygon_counts[i];
		Buffer<XPoint> out_points(n);
		XPoint *t = out_points;
		XPoint *e = t + n;
		while(t < e) {
			t->x = (short)(in->x + offset.x);
			t->y = (short)(in->y + offset.y);
			t++;
			in++;
		}
		XFillPolygon(Xdisplay, draw.GetDrawable(), draw.GetGC(), out_points, n, Nonconvex, CoordModeOrigin);
	}
}

void SystemDraw::DrawPolyPolyPolygonOp(const Point *vertices, int vertex_count,
	 const int *subpolygon_counts, int subpolygon_count_count,
	const int *disjunct_polygon_counts, int disjunct_polygon_count_count,
	Color color, int width, Color outline, uint64 pattern, Color doxor)
{
	DrawLock __;
	if(vertex_count == 0)
		return;

	bool is_xor = !IsNull(doxor);
	XGCValues gcv_old, gcv_new;
	XGetGCValues(Xdisplay, GetGC(), GCForeground | GCFunction | GCLineWidth, &gcv_old);
	unsigned xor_pixel = (is_xor ? GetXPixel(doxor) : 0);
	if(!IsNull(color))
	{
		gcv_new.foreground = GetXPixel(color) ^ xor_pixel;
		gcv_new.function = is_xor ? X11_ROP2_XOR : X11_ROP2_COPY;
		XChangeGC(Xdisplay, GetGC(), GCForeground | GCFunction, &gcv_new);
		DrawPolyPolyPolygonRaw(*this, vertices, vertex_count,
			subpolygon_counts, subpolygon_count_count,
			disjunct_polygon_counts, disjunct_polygon_count_count);
	}
	if(!IsNull(outline))
	{
		gcv_new.foreground = GetXPixel(outline) ^ xor_pixel;
		gcv_new.line_width = width;
		XChangeGC(Xdisplay, GetGC(), GCForeground | GCLineWidth, &gcv_new);
		Point offset = GetOffset();
		for(const int *sp = subpolygon_counts, *se = sp + subpolygon_count_count; sp < se; sp++)
		{
			Buffer<XPoint> segment(*sp + 1);
			XPoint *out = segment;
			for(const Point *end = vertices + *sp; vertices < end; vertices++, out++)
			{
				out -> x = (short)(vertices -> x + offset.x);
				out -> y = (short)(vertices -> y + offset.y);
			}
			*out = segment[0];
			XDrawLines(Xdisplay, GetDrawable(), GetGC(), segment, *sp + 1, CoordModeOrigin);
		}
	}
	XChangeGC(Xdisplay, GetGC(), GCForeground | GCFunction | GCLineWidth, &gcv_old);
}

void SystemDraw::DrawEllipseOp(const Rect& r, Color color, int pen, Color pencolor)
{
	DrawLock __;
	SetLineStyle(pen);
	if(!IsNull(color)) {
		SetForeground(color);
		XFillArc(Xdisplay, dw, gc, r.left + actual_offset.x, r.top + actual_offset.y,
			r.Width() - 1, r.Height() - 1, 0, 360 * 64);
	}
	if(!IsNull(pencolor) && !IsNull(pen)) {
		SetForeground(pencolor);
		XDrawArc(Xdisplay, dw, gc, r.left + actual_offset.x, r.top + actual_offset.y,
			r.Width() - 1, r.Height() - 1, 0, 360 * 64);
	}
}

void SystemDraw::DrawArcOp(const Rect& rc, Point start, Point end, int width, Color color)
{
	DrawLock __;
	XGCValues gcv, gcv_old;
	XGetGCValues(Xdisplay, GetGC(), GCForeground, &gcv_old);
	Point offset = GetOffset();
	gcv.foreground = GetXPixel(color);
	XChangeGC(Xdisplay, GetGC(), GCForeground, &gcv);
	Point centre = rc.CenterPoint();
	int angle1 = fround(360 * 64 / (2 * M_PI) *
	                    atan2(centre.y - start.y, start.x - centre.x));
	int angle2 = fround(360 * 64 / (2 * M_PI) *
	                    atan2(centre.y - end.y, end.x - centre.x));
	if(angle2 <= angle1)
		angle2 += 360 * 64;
	angle2 -= angle1;
	XDrawArc(Xdisplay, GetDrawable(), GetGC(), rc.left + offset.x, rc.top + offset.y,
		rc.Width(), rc.Height(), angle1, angle2);
	XChangeGC(Xdisplay, GetGC(), GCForeground, &gcv_old);
}

void SystemDraw::DrawTextOp(int x, int y, int angle, const wchar *text, Font font,
                      Color ink, int n, const int *dx) {
	LTIMING("DrawText");
	LLOG("DrawText " << ToUtf8(WString(text, n)) << " color:" << ink << " font:" << font);
	//TODO - X11 seems to crash when displaying too long strings (?)
	int ox = x + actual_offset.x;
	int oy = y + actual_offset.y;
	SetForeground(ink);
	SetFont(font, angle);
	const FontInfo::Data *fd = lastFont.ptr;
	XftColor c;
	c.color.red = ink.GetR() << 8;
	c.color.green = ink.GetG() << 8;
	c.color.blue = ink.GetB() << 8;
	c.color.alpha = 0xffff;
	c.pixel = GetXPixel(ink.GetR(), ink.GetG(), ink.GetB());
	if(angle) {
		int xpos = 0;
		for(int i = 0; i < n; i++) {
			wchar h = text[i];
			XftDrawString16(xftdraw, &c, fd->xftfont,
			                int(ox + xpos * fd->cosa + fd->offset.cx),
			                int(oy - xpos * fd->sina + fd->offset.cy),
			                (FcChar16 *)&h, 1);
			xpos += dx ? dx[i] : lastFont[text[i]];
		}
		if(font.IsUnderline() || font.IsStrikeout()) {
			x += fd->offset.cx;
			y += fd->offset.cy;
			if(font.IsUnderline())
				DrawLine(
					int(x + fd->underline_position * fd->sina),
					int(y + fd->underline_position * fd->cosa),
					int(x + xpos * fd->cosa + fd->underline_position * fd->sina),
					int(y - xpos * fd->sina + fd->underline_position * fd->cosa),
					fd->underline_thickness,
					ink
				);
			if(font.IsStrikeout()) {
				int p = 2 * fd->ascent / 3;
				DrawLine(
					int(x + p * fd->sina),
					int(y + p * fd->cosa),
					int(x + xpos * fd->cosa + p * fd->sina),
					int(y - xpos * fd->sina + p * fd->cosa),
					fd->underline_thickness,
					ink
				);
			}
		}
	}
	else {
		if(dx) {
			int xpos = ox;
			Buffer<XftCharSpec> ch(n);
			for(int i = 0; i < n; i++) {
				ch[i].ucs4 = text[i];
				ch[i].x = xpos;
				ch[i].y = oy + fd->ascent;
				xpos += dx[i];
			}
			XftDrawCharSpec(xftdraw, &c, fd->xftfont, ch, n);
		}
		else
			XftDrawString16(xftdraw, &c, fd->xftfont, ox, oy + fd->ascent,
			                (FcChar16 *)text, n);
		LLOG("XftColor: r=" << c.color.red << ", g=" << c.color.green << ", b=" << c.color.blue
			<< ", alpha=" << c.color.alpha << ", pixel=" << FormatIntHex(c.pixel));
		if(font.IsUnderline() || font.IsStrikeout()) {
			int cx;
			if(dx && n > 0) {
				cx = 0;
				Sum(cx, dx, dx + n - 1);
				cx += lastFont[text[n - 1]];
			}
			else
				cx = GetTextSize(text, font, n).cx;
			if(font.IsUnderline())
				DrawRect(x, y + lastFont.GetAscent() + lastFont.ptr->underline_position,
				         cx, lastFont.ptr->underline_thickness, ink);
			if(font.IsStrikeout())
				DrawRect(x, y + 2 * lastFont.GetAscent() / 3,
				         cx, lastFont.ptr->underline_thickness, ink);
		}
	}
}

#endif

END_UPP_NAMESPACE
