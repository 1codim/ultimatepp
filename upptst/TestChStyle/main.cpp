#include "TestChStyle.h"

void Dl(DropList& dl)
{
	dl.Add("Case1");
	dl.Add("Case2");
	dl <<= "Case1";
}

void Dc(WithDropChoice<EditString>& dc)
{
	dc.AddList("Case1");
	dc.AddList("Case2");
	dc <<= "Case1";
}

TestChStyle::TestChStyle()
{
	AddFrame(menu);
	menu.Sub("Something", [=](Bar& bar) {
		bar.Add("Open..", [] { SelectFileOpen("*"); });
		bar.Add("Information..", [] { PromptOK("information."); });
		bar.Add("Exclamation..", [] { Exclamation("exclamation!"); });
		bar.Add("Question..", [] { PromptYesNo("question?"); });
		bar.Add(false, "Disabled", [] {});
		static bool check;
		bar.Add("Check", [] { check = !check; }).Check(check);
		bar.Separator();
		bar.Sub("Submenu", [](Bar& bar) { bar.Add("Something", []{}); });
	});

	AddFrame(bar);
	bar.Set([](Bar& bar) { bar.Add(CtrlImg::Diskette(), [] {
		FileSelector fs;
		fs.AllFilesType();
		fs.ExecuteOpen();
	}).Tip("This is test"); });
	
	CtrlLayoutOKCancel(*this, "Window title");
	
	normal.NullText("Normal");
	readonly <<= "Read only";
	disabled <<= "Disabled";
	disabled.Disable();
	
	Dl(dl_normal);
	Dl(dl_readonly);
	Dl(dl_disabled);
	dl_disabled.Disable();

	Dc(dc_normal);
	Dc(dc_readonly);
	Dc(dc_disabled);
	dc_disabled.Disable();
	
	dis.Disable();
	
	option1.Disable();
	option1 <<= false;

	option2.Disable();
	option2 <<= Null;

	option3.Disable();
	option3 <<= true;
	
	switch1.Disable();
	switch1 <<= 1;

#ifdef CPP_11
	standard << [] { Ctrl::SetSkin(ChStdSkin); };
	classic << [] { Ctrl::SetSkin(ChClassicSkin); };
	host << [] { Ctrl::SetSkin(ChHostSkin); };
#endif

	for(int i = 0; i < 100; i++)
		tab.Add("Tab " + AsString(i));
	
	tm.Set(-20, [=] { pi.Set(GetTickCount() / 10 % 100, 100); });
	
	list.HeaderObject().Absolute();
	list.AddColumn("Col", 50).Sorting();
	list.AddColumn("Col", 50).Sorting();
	list.AddColumn("Col", 50).Sorting();
	list.AddColumn("Col", 50).Sorting();
	list.AddColumn("Col", 50).Sorting();
	list.Add("Some very very very long text to invoke DisplayPopup");
	for(int i = 0; i < 50; i++)
		list.Add(i);
	
	es_error.Error();
	es_error.NullText(CtrlImg::Computer());
	
	dc_error.Error();
	dc_error.NullText(CtrlImg::Computer());

	dc_error2.Error();
	dc_error2.NullText(CtrlImg::Computer());
	dc_error2.AddList("1");
	
	spin_sides.OnSides();
	
	dis1 <<= 12;
	dis1.Disable();

	dis2 <<= 12;
	dis2.OnSides();
	dis2.Disable();
	
	tm_dis.Disable();
	dt_dis.Disable();
}

GUI_APP_MAIN
{
	StdLogSetup(LOG_FILE|LOG_ELAPSED);
	Ctrl::SetDarkThemeEnabled();
//	Ctrl::SetSkin(ChStdSkin);

	RDUMP(SColorDisabled());
	RDUMP(SColorText());
	
	TestChStyle().Run();
}
