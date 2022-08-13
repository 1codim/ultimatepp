#ifndef _clang_clang_h
#define _clang_clang_h

#include <ide/Common/Common.h>
#include <clang-c/Index.h>

using namespace Upp;

class CoEvent {
	Mutex             lock;
	ConditionVariable cv;

public:
	void Wait(int timeout_ms)   { lock.Enter(); cv.Wait(lock, timeout_ms); lock.Leave(); }
	void Wait()                 { lock.Enter(); cv.Wait(lock); lock.Leave(); }
	void Broadcast()            { cv.Broadcast(); }
};

String FetchString(CXString cs);
String GetCursorKindName(CXCursorKind cursorKind);
String GetCursorSpelling(CXCursor cursor);
String GetTypeSpelling(CXCursor cursor);

struct SourceLocation : Moveable<SourceLocation> {
	String path;
	Point  pos;
	
	bool operator==(const SourceLocation& b) const { return path == b.path && pos == b.pos; }
	bool operator!=(const SourceLocation& b) const { return !operator==(b); }
	void Serialize(Stream& s)                      { s % path % pos; }
	hash_t GetHashValue() const                    { return CombineHash(path, pos); }
	String ToString() const                        { return path + ": " + AsString(pos); }
};

String RedefineMacros();
String GetClangInternalIncludes();

enum AdditionalKinds {
	KIND_INCLUDEFILE = -1000,
	KIND_INCLUDEFILE_ANY,
	KIND_INCLUDEFOLDER,
	KIND_COMPLETE,
};

Image  CxxIcon(int kind); // TODO: Move here
int    PaintCpp(Draw& w, const Rect& r, int kind, const String& name, const String& pretty, Color ink, bool focuscursor, bool retval_last = false);
String SignatureQtf(const String& name, const String& pretty, int pari);

bool IsStruct(int kind);
bool IsTemplate(int kind);
bool IsFunction(int kind);
bool IsVariable(int kind);
int  FindId(const String& s, const String& id);

enum {
	ITEM_TEXT,
	ITEM_NAME,
	ITEM_OPERATOR,
	ITEM_CPP_TYPE,
	ITEM_CPP,
	ITEM_PNAME,
	ITEM_TNAME,
	ITEM_NUMBER,
	ITEM_SIGN,
	ITEM_UPP,
	ITEM_TYPE,
	
	ITEM_PTYPE = ITEM_TYPE + 10000,
};

struct ItemTextPart : Moveable<ItemTextPart> {
	int pos;
	int len;
	int type;
	int ii;
	int pari;
};

Vector<ItemTextPart> ParsePretty(const String& name, const String& signature, int *fn_info = NULL);

struct AutoCompleteItem : Moveable<AutoCompleteItem> {
	String parent;
	String name;
	String signature; // TODO: rename to pretty
	int    kind;
	int    priority;
};

struct AnnotationItem : Moveable<AnnotationItem> {
	int    kind;
	Point  pos;
	bool   definition;
	bool   isvirtual;
	String name; // Method
	String type; // for String x, Upp::String, surely valid for variables only
	String id; // Upp::Class::Method(Upp::Point p)
	String pretty; // void Class::Method(Point p)
	String nspace; // Upp
	String uname; // METHOD
	String nest; // Upp::Class
	String unest; // UPP::CLASS
	String bases; // base classes of struct/class
	
	void Serialize(Stream& s);
};

String GetClass(const AnnotationItem& m);
String MakeDefinition(const AnnotationItem& m);

struct ReferenceItem : Moveable<ReferenceItem> {
	String id;
	Point  pos;
	
	bool operator==(const ReferenceItem& b) const { return id == b.id && pos == b.pos; }
	hash_t GetHashValue() const                   { return CombineHash(id, pos); }
	
	void Serialize(Stream& s);
};

struct CurrentFileContext {
	String                   filename;
	String                   real_filename; // in case we need to present .h as .cpp
	int                      line_delta = 0; // in case we need to present .h as .cpp
	String                   includes;
	String                   defines;
	String                   content;
};

struct CppFileInfo : Moveable<CppFileInfo> {
	Vector<AnnotationItem> items;
	Vector<ReferenceItem>  refs;
};

enum { PARSE_FILE = 0x80000000 };

struct Clang {
	CXIndex           index = nullptr;
	CXTranslationUnit tu = nullptr;

	void Dispose();
	bool Parse(const String& filename, const String& content, const String& includes, const String& defines, dword options);
	bool ReParse(const String& filename, const String& content);
	
	Clang();
	~Clang();
};

void DumpDiagnostics(CXTranslationUnit tu);

String CleanupId(const char *s);
String CleanupPretty(const String& signature);

bool   IsCppSourceFile(const String& path);
bool   IsSourceFile(const String& path);
bool   IsHeaderFile(const String& path);

class ClangVisitor {
	bool initialized = false;
	CXPrintingPolicy pp_id, pp_pretty;
	
	bool           ProcessNode(CXCursor c);
	SourceLocation GetLocation(CXSourceLocation c);

	friend CXChildVisitResult clang_visitor(CXCursor cursor, CXCursor p, CXClientData clientData);

	VectorMap<CXFile, String>                 cxfile; // accelerate CXFile (CXFile has to be valid across tree as there is no Dispose)
	VectorMap<String, Index<ReferenceItem>>   ref_done; // avoid self-references, multiple references
	
	struct MCXCursor : Moveable<MCXCursor> {
		CXCursor cursor;
	};
	
	ArrayMap<SourceLocation, MCXCursor>  tfn; // to convert e.g. Index<String>::Find(String) to Index::Find(T)

public:
	VectorMap<String, CppFileInfo> info;
	
	Gate<const String&> WhenFile;

	void Do(CXTranslationUnit tu);
	~ClangVisitor();
};

void SetCurrentFile(const CurrentFileContext& ctx, Event<const CppFileInfo&> done);
bool IsCurrentFileParsing();
void CancelCurrentFile();
bool IsCurrentFileDirty();

void SetAutoCompleteFile(const CurrentFileContext& ctx);
bool IsAutocompleteParsing();
void StartAutoComplete(const CurrentFileContext& ctx, int line, int column, bool macros,
                       Event<const Vector<AutoCompleteItem>&> done);
void CancelAutoComplete();

String FindMasterSource(PPInfo& ppi, const Workspace& wspc, const String& header_file);

struct FileAnnotation0 {
	String defines = "<not_loaded>";
	String includes;
	String master_file;
	Time   time = Time::Low();
};

struct FileAnnotation : FileAnnotation0, CppFileInfo {
	void Serialize(Stream& s);
};

ArrayMap<String, FileAnnotation>& CodeIndex();

class Indexer {
	struct Job : Moveable<Job> {
		String                                  path;
		String                                  blitz;
		String                                  includes;
		String                                  defines;
		WithDeepCopy<VectorMap<String, Time>>   file_times;
		WithDeepCopy<VectorMap<String, String>> master_files;
	};

	static CoEvent            event, scheduler;
	static Mutex              mutex;
	static Vector<Job>        jobs;
	static int                jobi;
	static std::atomic<int>   running_indexers;
	static bool               running_scheduler;
	static String             main, includes, defines;
	
	static void IndexerThread();
	static void SchedulerThread();

public:
	static void Start(const String& main, const String& includes, const String& defines);
	static bool IsRunning();
};

#endif
