#define NDL_USE_BCTLIB
#define NDL_USE_ANALYZELIB
#define NDL_USE_DCMTK
#define NDL_USE_JPEG
#define NDL_USE_BMP
#define NDL_USE_WXWIDGETS

#include <image.h>
#include <map>

#define APPNAME "NVOL"

using namespace ndl;

wxProgressDialog* g_progressDlg;
void wxprogressstart(){
    if (g_progressDlg==0){
        g_progressDlg = new wxProgressDialog(_("Please wait"), _("................................................"), 100, wxTheApp->GetTopWindow(), wxPD_AUTO_HIDE | wxPD_REMAINING_TIME);
    }
}
void wxprogressupdate(char* text,int i){
    g_progressDlg->Update(i,text);
    g_progressDlg->Refresh();
}
void wxprogressend(){
    g_progressDlg->Destroy();
    delete g_progressDlg;
    g_progressDlg = 0;
}
template<class VoxelType,int DIM>
void wxsetupprogress(Image<VoxelType,DIM>& im){
    im.setprogressupdatefunction(&wxprogressupdate);
    im.setprogressstartfunction(&wxprogressstart);
    im.setprogressendfunction(&wxprogressend);
}

template<class VoxelType,int DIM>
bool closeevent(ViewInfo<VoxelType,DIM>* view){
    //save view settings for next time before closing
    long showcrossheirs = view->showcrossheirs;
    wxConfig myconfig(APPNAME);
    myconfig.Write("showcrossheirs", showcrossheirs );
    
    //printf("I DON'T WANT TO CLOSE\n");
    //return false;
    return true;
}

template<class VoxelType,int DIM>
int createimage(Image<VoxelType,DIM>*& impointer,std::string inputfile,std::string loadparams,bool seqflag,int interptype){
    printf("DIM: %d, VoxelType: %s             \r",DIM,getdatatypestring(getdatatype<VoxelType>()).c_str());
    Image<VoxelType,DIM>* blah = new Image<VoxelType,DIM>();
    wxsetupprogress(*blah);
    blah->loadfile(inputfile.c_str(),loadparams.c_str(),seqflag);
    if (blah->loaded()==false) return 0; 
    if (interptype==NDL_NONE || blah->isisotropic()){
        impointer = blah;
    } else {
        VoxelType clampmin = blah->m_min;
        VoxelType clampmax = blah->m_max;
        impointer = new Image<VoxelType,DIM>();
        blah->scaleisotropic();
        blah->transform(*impointer,clampmin,clampmax,interptype);
        impointer->m_min=blah->m_min;
        impointer->m_max=blah->m_max;
        delete blah;
    }
    
    //determine view settings
    std::string viewsettings;
    long showcrossheirs = 1;
    wxConfig myconfig(APPNAME);
    myconfig.Read("showcrossheirs", &showcrossheirs);
    if (showcrossheirs) viewsettings += "-showcrossheirs ";
    
    //add the view
    ViewInfo<VoxelType,DIM>* vi = impointer->addview(viewsettings.c_str());
    vi->closeevent = closeevent<VoxelType,DIM>;
    return vi->pid;
}

class document {
    public:
    document(std::string inputfile,std::string loadparams,int seqflag,int interptype){
        //save vars
        m_inputfile = inputfile;
        m_loadparams = loadparams + " -exactmatchonly";
        m_seqflag = seqflag;
        m_interptype = interptype;
        m_pid = 0;
        
        //init pointers
        im2c=0;
        im2uc=0;
        im2s=0;
        im2us=0;
        im2i=0;
        im2ui=0;
        im2ll=0;
        im2ull=0;
        im2f=0;
        im2d=0;
        im3c=0;
        im3uc=0;
        im3s=0;
        im3us=0;
        im3i=0;
        im3ui=0;
        im3ll=0;
        im3ull=0;
        im3f=0;
        im3d=0;
        m_pid=0;

        if (!m_pid) m_pid = createimage(im2f,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im2ui,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im2i,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im2ull,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im2ll,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im2us,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im2s,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im2uc,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im2c,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im2d,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3f,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3ui,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3i,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3ull,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3ll,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3us,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3s,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3uc,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3c,m_inputfile,m_loadparams,m_seqflag,m_interptype);
        if (!m_pid) m_pid = createimage(im3d,m_inputfile,m_loadparams,m_seqflag,m_interptype);
    }
    ~document(){
        //cleanup
        delete im2c;
        delete im2uc;
        delete im2s;
        delete im2us;
        delete im2i;
        delete im2ui;
        delete im2ll;
        delete im2ull;
        delete im2f;
        delete im2d;
        delete im3c;
        delete im3uc;
        delete im3s;
        delete im3us;
        delete im3i;
        delete im3ui;
        delete im3ll;
        delete im3ull;
        delete im3f;
        delete im3d;
    }
    
    int save(std::string docname,std::string paramstring,int seqflag=-1){
        if (im2c) return im2c->savefile(docname.c_str(),paramstring,seqflag);
        else if (im2uc) return im2uc->savefile(docname.c_str(),paramstring,seqflag);
        else if (im2s) return im2s->savefile(docname.c_str(),paramstring,seqflag);
        else if (im2us) return im2us->savefile(docname.c_str(),paramstring,seqflag);
        else if (im2i) return im2i->savefile(docname.c_str(),paramstring,seqflag);
        else if (im2ui) return im2ui->savefile(docname.c_str(),paramstring,seqflag);
        else if (im2ll) return im2ll->savefile(docname.c_str(),paramstring,seqflag);
        else if (im2ull) return im2ull->savefile(docname.c_str(),paramstring,seqflag);
        else if (im2f) return im2f->savefile(docname.c_str(),paramstring,seqflag);
        else if (im2d) return im2d->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3c) return im3c->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3uc) return im3uc->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3s) return im3s->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3us) return im3us->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3i) return im3i->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3ui) return im3ui->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3ll) return im3ll->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3ull) return im3ull->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3f) return im3f->savefile(docname.c_str(),paramstring,seqflag);
        else if (im3d) return im3d->savefile(docname.c_str(),paramstring,seqflag);
        return 0;
    }
    
    std::string m_inputfile;
    std::string m_loadparams;
    int m_dim;
    int m_seqflag;
    int m_interptype;
    int m_pid;
    
    private:
    //2D images
    Image<char,2>* im2c;
    Image<unsigned char,2>* im2uc;
    Image<short,2>* im2s;
    Image<unsigned short,2>* im2us;
    Image<int,2>* im2i;
    Image<unsigned int,2>* im2ui;
    Image<long long,2>* im2ll;
    Image<unsigned long long,2>* im2ull;
    Image<float,2>* im2f;
    Image<double,2>* im2d;
    
    //3D images
    Image<char,3>* im3c;
    Image<unsigned char,3>* im3uc;
    Image<short,3>* im3s;
    Image<unsigned short,3>* im3us;
    Image<int,3>* im3i;
    Image<unsigned int,3>* im3ui;
    Image<long long,3>* im3ll;
    Image<unsigned long long,3>* im3ull;
    Image<float,3>* im3f;
    Image<double,3>* im3d;
};

class OpenSettings {
    public:
        
    long interptype;
    wxString extrasettingsstring;
    wxConfig myconfig;
        
    OpenSettings()
    : myconfig(APPNAME) 
    {
        interptype=NDL_NONE;
        extrasettingsstring="";
        reloadvalues();
    }
    void reloadvalues(){
        myconfig.Read("open_interptype", &interptype);
        myconfig.Read("open_extrasettings", &extrasettingsstring);
    }
    
    void savevalues(){
        myconfig.Write("open_interptype", interptype );
        myconfig.Write("open_extrasettings", extrasettingsstring );
    }
    
    void printvalues(){
        wxString str;
        wxMessageBox(wxString::Format("interptype: %d, extrasettingsstring: %s", interptype,extrasettingsstring), _("values"),wxOK | wxICON_INFORMATION, NULL);
    }
};

class OpenSettingsDialog : public wxDialog {
    //wxID_HIGHEST < enums < wxID_HIGHEST + 10 are reserved for OpenSettingsDialog
    enum {ID_COMBO=wxID_HIGHEST+1};
    wxComboBox* interpcombo;
    wxCheckBox* sequencebutton;
    wxTextCtrl* extrasettingstext;
    wxGridSizer* gridsizer;
    wxStaticText* interptypetext;
    wxStaticText* extrasettingslabel;
    wxBoxSizer *mainsizer;
    
    public:
    OpenSettings opensettings;
    OpenSettingsDialog(wxWindow *parent,bool extrasettings=false) : wxDialog(parent, wxID_ANY, wxString(_T("Open File Settings"))){
        int sizerborder = 5;
        int sizerflags = wxALIGN_CENTER | wxALL | wxEXPAND;
        
        //create a 2 column grid sizer
        gridsizer = new wxGridSizer(2);
        
        //add interpolation combobox
        interptypetext = new wxStaticText(this,wxID_ANY,_T("resize to isotropic:"));
        const wxString interp_choices[] = {_("NONE"),_("NEAREST NEIGHBOR"),_("LINEAR"),_("CUBIC")};
        
        wxString defaultchoice=interp_choices[0];
        if (opensettings.interptype==NDL_NONE) defaultchoice=interp_choices[0];
        if (opensettings.interptype==NDL_NN) defaultchoice=interp_choices[1];
        if (opensettings.interptype==NDL_LINEAR) defaultchoice=interp_choices[2];
        if (opensettings.interptype==NDL_CUBIC) defaultchoice=interp_choices[3];
        
        interpcombo = new wxComboBox(this,ID_COMBO,defaultchoice,wxDefaultPosition,wxDefaultSize,4,interp_choices,wxCB_READONLY);
        gridsizer->Add(interptypetext, 0, sizerflags, sizerborder);
        gridsizer->Add(interpcombo, 0, sizerflags, sizerborder);
        
        if (extrasettings){
            extrasettingslabel = new wxStaticText(this,wxID_ANY,_T("extra settings:"));
            extrasettingstext = new wxTextCtrl(this,wxID_ANY,opensettings.extrasettingsstring);
            gridsizer->Add(extrasettingslabel, 0, sizerflags, sizerborder);
            gridsizer->Add(extrasettingstext, 0, sizerflags, sizerborder);
        } else extrasettingstext = 0;

        //create the buttons sizer
        wxSizer* buttonSizer = CreateButtonSizer(wxOK | wxCANCEL);
        
        //Create the main sizer and apply it to the dialog
        mainsizer = new wxBoxSizer(wxVERTICAL);
        mainsizer->Add(gridsizer,0, sizerflags, sizerborder);
        if (buttonSizer) mainsizer->Add(buttonSizer);
        
        SetSizer(mainsizer);
        mainsizer->SetSizeHints(this);
        mainsizer->Fit(this);
        
        Connect(wxID_OK,wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(OpenSettingsDialog::OnOk));
    }
    void OnOk(wxCommandEvent& event){
        opensettings.reloadvalues();
        if (interpcombo->GetValue()==_("NONE")) opensettings.interptype=NDL_NONE;
        if (interpcombo->GetValue()==_("NEAREST NEIGHBOR")) opensettings.interptype=NDL_NN;
        if (interpcombo->GetValue()==_("LINEAR")) opensettings.interptype=NDL_LINEAR;
        if (interpcombo->GetValue()==_("CUBIC")) opensettings.interptype=NDL_CUBIC;
        if (extrasettingstext) opensettings.extrasettingsstring=extrasettingstext->GetValue();
        opensettings.savevalues();
        event.Skip();
        
        //~ delete interpcombo;
        //~ delete sequencebutton;
        //~ delete extrasettingstext;
        //~ delete gridsizer;
        //~ delete interptypetext;
        //~ delete extrasettingslabel;
        //~ delete mainsizer;
    }
    
};

class MyFrame : public wxFrame {
    //enums >= wxID_HIGHEST + 100 are reserved for myframe
    enum {ID_Quit=wxID_HIGHEST + 100, ID_About, ID_Open, ID_OpenSeq, ID_OpenSettings, ID_Save, ID_SaveSeq, ID_CommandTimer, ID_Window /* should be last one */ };
    std::map< int,document* > docs;
    document* activedoc;
    wxMenu* menuSettings;
    wxMenu* menuWindow;
    wxMenu* menuFile;
    wxFileHistory fileHistory;

    int winnum;

    public:
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame((wxFrame*)NULL, -1, title, pos, size), fileHistory(5)
    {
        activedoc = 0;
        winnum = 0;
        
        //setup menus
        wxMenuBar* menuBar = new wxMenuBar;

        //file menu
        menuFile = new wxMenu;
        menuFile->Append(ID_Open, "&Open");
        menuFile->Append(ID_OpenSeq, "&Open Sequence");
        menuFile->Append(ID_Save, "&Save As");
        menuFile->Append(ID_SaveSeq, "&Save Sequence As");
        menuFile->AppendSeparator();
        menuFile->Append(ID_About, "&About...");
        menuFile->Append(ID_Quit, "E&xit");
        menuBar->Append(menuFile, "&File");
        fileHistory.UseMenu(menuFile);
        OpenSettings opensettings;
        fileHistory.Load(opensettings.myconfig);
        int numfiles = fileHistory.GetCount();
        for(int i=0;i<numfiles;i++) Connect(wxID_FILE1+i,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnRecentFile));
        
        //window menu
        menuWindow = new wxMenu;
        menuBar->Append(menuWindow, "&Window");
        
        //settings menu
        menuSettings = new wxMenu;
        menuSettings->Append(ID_OpenSettings, "&Open File Settings");
        menuBar->Append(menuSettings, "&Settings");
        
        //add the menubar
        SetMenuBar(menuBar);

        // connect event handlers
        Connect(ID_Quit,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnQuit));
        Connect(ID_About,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnAbout));
        Connect(ID_Open,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnOpen));
        Connect(ID_OpenSeq,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnOpenSeq));
        Connect(ID_OpenSettings,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnOpenSettings));
        Connect(ID_Save,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnSave));
        Connect(ID_SaveSeq,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnSaveSeq));
        Connect(ID_CommandTimer,wxEVT_TIMER, wxTimerEventHandler(MyFrame::OnTimer));
        Connect(wxID_ANY,wxEVT_MENU_OPEN,wxMenuEventHandler(MyFrame::OnMenuOpen));

      //CreateStatusBar();
      //SetStatusText("Welcome to wxWidgets");

        //add panel
        wxPanel* windowtext = new wxPanel(this);
        
        //~ wxTextCtrl *windowtext = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
        //~ windowtext->Show();
        //~ wxStreamToTextRedirector redir(windowtext);
        
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(windowtext, 1, wxALL | wxEXPAND);
        SetSizer(sizer);
        
        //setup icon
        if (wxFileExists("nvol.ico")) SetIcon(wxIcon("nvol.ico",wxBITMAP_TYPE_ICO));
        
        //setup timer
        wxTimer* timer = new wxTimer(this, ID_CommandTimer);
        timer->Start(400);
    }
    
    ~MyFrame(){
        for(std::map< int,document* >::iterator i = docs.begin(); i != docs.end(); ++i){
            document* thedoc = (*i).second;
            delete thedoc;
        }
    }
    
    void updateprocessinfo(){
        for(std::map< int,document* >::iterator i = docs.begin(); i != docs.end(); i++){
            document* thedoc = (*i).second;
            if (isactiveprocess(thedoc->m_pid) || activedoc==0) activedoc=thedoc;
            if (!isprocessrunning(thedoc->m_pid)){
                //remove from the map
                docs.erase(i);
                
                //clear active doc if need be
                if (thedoc==activedoc) activedoc=0;
                
                //delete the doc
                delete thedoc;
                updateprocessinfo(); //recurse to handle any additional docs (this is kind of ghetto, but it works)
                break;
            }
        }
    }
    void OnTimer(wxTimerEvent& event){ updateprocessinfo(); }
    
    void OnWindowSelected(wxCommandEvent& event){
        int thewinnum = event.GetId() - ID_Window;
        activateprocess(docs[thewinnum]->m_pid);
    }

    void OnMenuOpen(wxMenuEvent& event){
        //get latest updates on processor state
        updateprocessinfo();
        
        //clear the window menu
        do {
            wxMenuItem* menuitem = menuWindow->FindItemByPosition(0);
            menuWindow->Delete(menuitem);
            if (menuitem==0) break;
        } while(1);
        
        //rebuild the window menu
        for(std::map< int,document* >::iterator i = docs.begin(); i != docs.end(); ++i){
            document* thedoc = (*i).second;
            int thewindowid = ID_Window + (*i).first;
            wxMenuItem* menuitem=menuWindow->AppendRadioItem( thewindowid, wxString::Format("%s", thedoc->m_inputfile.c_str()) );
            Connect(thewindowid,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnWindowSelected));
            if (thedoc==activedoc) menuWindow->FindChildItem(thewindowid)->Check(true);
        }
        
        //set enabled state of menu items
        menuFile->Enable(ID_Save,(bool)activedoc);
        menuFile->Enable(ID_SaveSeq,(bool)activedoc);
    }

    void OnQuit(wxCommandEvent& WXUNUSED(event)){
       Close(TRUE);
    }

    void OnAbout(wxCommandEvent& WXUNUSED(event)){
        wxMessageBox( _("NVol is an open source ND image viewer developed using NDL, an N-Dimensional Image Library. It was written by Nathan Packard and is released under the LGPL license V.3 (copyright 2010)"), _("About NVol"),wxOK | wxICON_INFORMATION, this);
    }
    
    // Creates a Save Dialog
    void save(bool seqflag=false){
        if (!activedoc) return;
        //figure out file filter
        wxString filefilter="";
        std::vector<std::string> imagetypes = NDL_getimagetypes();
        std::vector<std::string> saveextensions = NDL_getimagesaveextensions();
        for(int i=0;i<imagetypes.size();i++){
            filefilter+=imagetypes[i].c_str();
            if (saveextensions[i]=="") filefilter+="|*.*|";
            else {
                filefilter+="|*.";
                filefilter+=saveextensions[i];
                filefilter+="|";
            }
        } //example string: filefilter = _("Breast CT Files|*.0001|C++ Source Files (*.cpp)|*.cpp| C Source files (*.c)|*.c|C header files (*.h)|*.h");
     
        // Creates a Save Dialog
        wxString dialogtitle = _("Save File As");
        if (seqflag) dialogtitle = _("Save File Sequence As");
        wxFileDialog *SaveDialog = new wxFileDialog(this, dialogtitle, wxEmptyString, wxEmptyString, filefilter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);
        //SaveDialog->SetFilterIndex(myimage->displayimagep->m_imagepluginindex);
        if (SaveDialog->ShowModal() == wxID_OK){
            wxString CurrentDocPath = SaveDialog->GetPath();
            std::string paramstring = "-filetype:";
            paramstring+=imagetypes[SaveDialog->GetFilterIndex()];
            
            //do the save
            if (activedoc->save(CurrentDocPath.c_str(),paramstring,seqflag)){
                activedoc->m_inputfile = CurrentDocPath.c_str();
            }
        }
        delete SaveDialog;
    }
    void OnSave(wxCommandEvent& WXUNUSED(event)){ save(); }
    void OnSaveSeq(wxCommandEvent& WXUNUSED(event)){ save(true); }

    void OnOpenSettings(wxCommandEvent& WXUNUSED(event)){
        OpenSettingsDialog dlg(this);
        dlg.ShowModal();
    }
    
    void open(wxString CurrentDocPath,bool seqflag=false){
        //load open settings
        OpenSettings opensettings;
        
        //open the document
        activedoc = new document(CurrentDocPath.c_str(),"",seqflag,opensettings.interptype);
        if (activedoc->m_pid){
            docs[winnum++] = activedoc;
            if (seqflag) fileHistory.AddFileToHistory(CurrentDocPath + _(" :SEQ"));
            else fileHistory.AddFileToHistory(CurrentDocPath);
            Connect(wxID_FILE1+fileHistory.GetCount()-1,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnRecentFile));
            fileHistory.Save(opensettings.myconfig);
        }
        else {
            delete activedoc;
            OpenSettingsDialog dlg(this,true);
            if (dlg.ShowModal() == wxID_OK){
                activedoc = new document(CurrentDocPath.c_str(),dlg.opensettings.extrasettingsstring.c_str(),seqflag,opensettings.interptype);
                if (activedoc->m_pid){
                    docs[winnum++] = activedoc;
                    if (seqflag) fileHistory.AddFileToHistory(CurrentDocPath + _(" :SEQ"));
                    else fileHistory.AddFileToHistory(CurrentDocPath);
                    Connect(wxID_FILE1+fileHistory.GetCount()-1,wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnRecentFile));
                    fileHistory.Save(dlg.opensettings.myconfig);
                }
                else {
                    delete activedoc;
                    wxMessageBox(wxString::Format("Couldn't Load File: %s\n",CurrentDocPath), _("values"),wxOK | wxICON_INFORMATION, NULL);
                }
            }
        }
    }
    
    // Creates an Open Dialog
    void opendialog(bool seqflag=false){
        //figure out file filter
        wxString filefilter="*.*|*.*|";
        std::vector<std::string> imagetypes = NDL_getimagetypes();
        std::vector<std::string> openextensions = NDL_getimageopenextensions();
        for(int i=0;i<imagetypes.size();i++){
            filefilter+=imagetypes[i].c_str();
            if (openextensions[i]=="") filefilter+="|*.*|";
            else {
                filefilter+="|*.";
                filefilter+=openextensions[i];
                filefilter+="|";
            }
        }
        
        wxString dialogtitle = _("Open File");
        if (seqflag) dialogtitle = _("Open File Sequence");
        wxFileDialog *OpenDialog = new wxFileDialog(this, dialogtitle, wxEmptyString, wxEmptyString, filefilter, wxFD_OPEN, wxDefaultPosition);
        if (OpenDialog->ShowModal() == wxID_OK){
            open(OpenDialog->GetPath(),seqflag);
        }
        delete OpenDialog;
    }
    void OnOpen(wxCommandEvent& WXUNUSED(event)){ opendialog(); }
    void OnOpenSeq(wxCommandEvent& WXUNUSED(event)){ opendialog(true); }
    void OnRecentFile(wxCommandEvent& event){
        wxString filename = fileHistory.GetHistoryFile(event.GetId()-wxID_FILE1);
        wxString t = _(" :SEQ");
        if (filename.Right(t.Length()) == t){
            filename = filename.Left(filename.Length() - t.Length());
            open(filename,true);
        } else open(filename);
    }
};

class MyApp: public wxApp{
    public:
    bool OnInit(){
        g_progressDlg = 0;
        MyFrame* myframe = new MyFrame( _(APPNAME), wxPoint(30, 30),wxSize(450,240) );
        myframe->Show();
        SetTopWindow(myframe);
        return true;
    } 
};


//~ //USE THIS FOR NO COMMAND PROMPT
//~ IMPLEMENT_APP(MyApp)

//USE THIS FOR A COMMAND PROMPT
int main(int argc, char *argv[]){
    MyApp* myapp = new MyApp();
    wxApp::SetInstance( myapp );
    wxEntryStart( argc, argv );
    wxTheApp->OnInit();
    wxTheApp->OnRun();
    wxTheApp->OnExit();
    wxEntryCleanup();
	return 0;
}
