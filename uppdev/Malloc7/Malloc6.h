#define V6

struct MSmall {
	enum { CACHE = 16 };
	
	struct FreeLink {
		FreeLink *next;
	};
	
	struct Page {
		Page        *next;
		Page        *prev;
		byte         klass;
		byte         active;
		MSmall      *heap;
		FreeLink    *freelist;
		
		void         LinkSelf()       { Dbl_Self(this); }
		void         Unlink()         { Dbl_Unlink(this); }
		void         Link(Page *lnk)  { Dbl_LinkAfter(this, lnk);  }
	
		void         Format(int k);

		byte *Begin()                 { return (byte *)this + sizeof(Page); }
		byte *End()                   { return (byte *)this + 4096; }
	};
	
	static int Ksz(int k)  { return (k + 1) << 4; }


	static Page       *global_empty[16];
	static StaticMutex global_lock;

	Page      full[16][1];
	Page      work[16][1];
	Page     *empty[16];
	void     *cache[16][CACHE];
	int       level[16];
	
	void  Init();

	void *AllocK(int k);
	void  FreeK(void *ptr, Page *page, int k);
	void *Alloc(size_t sz);
	void  Free(void *ptr);

	static void *CheckFree(void *p, int k);
	static void  FillFree(void *ptr, int k);

	void  Check();
};
