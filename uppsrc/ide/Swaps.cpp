#include "ide.h"

#define LLOG(x)  // DLOG(x)

void Ide::SwapS()
{
	if(designer)
		return;
	if(!editor.WaitCurrentFile())
		return;
	AnnotationItem cm = editor.FindCurrentAnnotation();
	struct Sf : Moveable<Sf> {
		String path;
		Point  pos;
		bool   definition;
	};
	Vector<Sf> set, list;
	for(int pass = 0; pass < 4 && set.GetCount() < 2; pass++) {
		set.Clear();
		for(const auto& f : ~CodeIndex())
			for(const AnnotationItem& m : f.value.items) {
				if(findarg(pass, 0, 3) >= 0 ? m.id == cm.id : // 3 in case nothing was found in pass 2
				   pass == 1 ? m.nest == cm.nest && m.name == cm.name :
				               m.nest == cm.nest && IsStruct(m.kind) && cm.nest.GetCount()) {
					Sf& sf = set.Add();
					sf.path = f.key;
					sf.pos = m.pos;
					sf.definition = m.definition;
				}
				if(set.GetCount() > 20) // sanity
					break;
			}
		if(set.GetCount() > 1)
			break;
	}
	
	if(set.GetCount() == 0)
		return;

	Sort(set, [](const Sf& a, const Sf& b) {
		return CombineCompare(a.path, b.path)(a.pos.y, b.pos.y)(a.pos.x, b.pos.x) < 0;
	});
	
	bool deff = false;
	
	for(;;) { // create a list of candidates alternating definition and declaration
		int q = FindMatch(set, [&](const Sf& a) { return a.definition == deff; });
		if(q >= 0) {
			list.Add(set[q]);
			set.Remove(q);
		}
		else
			break;
		deff = !deff;
	}
	list.Append(set); // add remaining items
	int q = max(FindMatch(list, [&](const Sf& a) { return a.path == editfile && a.pos.y == editor.GetCurrentLine(); }), 0);
	q = (q + 1) % list.GetCount();
	GotoPos(list[q].path, list[q].pos);
}

String GetFileLine(const String& path, int linei)
{
	static String         lpath;
	static Vector<String> line;
	if(path != lpath) {
		lpath = path;
		FileIn in(path);
		line.Clear();
		if(in.GetSize() < 1000000)
			while(!in.IsEof())
				line.Add(in.GetLine());
	}
	return linei >= 0 && linei < line.GetCount() ? line[linei] : String();
}

void Ide::References()
{
	if(designer)
		return;
	if(!editor.WaitCurrentFile())
		return;
	AnnotationItem cm = editor.FindCurrentAnnotation();
	SetFFound(ffoundi_next);
	FFound().Clear();
	for(const auto& f : ~CodeIndex())
		for(const ReferenceItem& m : f.value.refs)
			if(m.id == cm.id) {
				String ln = GetFileLine(f.key, m.pos.y);
				int count = 0;
				int pos = 0;
				if(cm.name.GetCount()) {
					pos = ::FindId(ln + m.pos.x, cm.name);
					if(pos >= 0)
						count = cm.name.GetCount();
					else
						pos = 0;
				}
				AddFoundFile(f.key, m.pos.y + 1, ln, pos, count);
			}
	FFoundFinish();
}
