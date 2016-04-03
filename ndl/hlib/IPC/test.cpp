#include "IPC.h"
#include "sharedmem.h"
#include <time.h>
#include <string>










#include <wx/wx.h>
 
class MyApp : public wxApp
{
   virtual bool OnInit();
};
 
class MyFrame : public wxFrame
{
public:
   MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
protected:
   // Do we really need to expose the implementation detail? I guess not.
   void OnQuit(wxCommandEvent& event);
   void OnAbout(wxCommandEvent& event);
private:
   enum {ID_Quit=wxID_HIGHEST + 1, ID_About};
};
 
bool MyApp::OnInit()
{
   wxFrame *frame = new MyFrame("Hello World", wxPoint(50,50),
     wxSize(450,350));
     frame->Show(TRUE);
     SetTopWindow(frame);
     return TRUE;
}
 
MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame((wxFrame*)NULL, -1, title, pos, size)
{
  // create menubar
  wxMenuBar* menuBar = new wxMenuBar;
  // create menu
  wxMenu* menuFile = new wxMenu;
  // append menu entries
  menuFile->Append(ID_About, "&About...");
  menuFile->AppendSeparator();
  menuFile->Append(ID_Quit, "E&xit");
  // append menu to menubar
  menuBar->Append(menuFile, "&File");
  // set frame menubar
  SetMenuBar(menuBar);
 
  // connect event handlers
  Connect(ID_Quit,wxEVT_COMMAND_MENU_SELECTED,
     wxCommandEventHandler(MyFrame::OnQuit));
  Connect(ID_About,wxEVT_COMMAND_MENU_SELECTED,
     wxCommandEventHandler(MyFrame::OnAbout));
 
  CreateStatusBar();
  SetStatusText("Welcome to wxWidgets");
}
 
void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
   Close(TRUE);
}
 
void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
   wxMessageBox("wxWidgets Hello World example.","About Hello World",
        wxOK|wxICON_INFORMATION, this);
}















#define MAXDIM 100
struct ViewInfo{
    int nDIM;
    char dataname[sizeof(void*)+1];
    int datatype;
    int ndim;
    int ncolors;
    int dim[MAXDIM];
    float size[MAXDIM];
};

int testview(char* message){

    MyApp* myapp = new MyApp();
    wxApp::SetInstance( myapp );
    int dummyargc=1;char** dummyargv=0;
    wxEntryStart( dummyargc, dummyargv );
    wxTheApp->OnInit();
    wxTheApp->OnRun();
    wxTheApp->OnExit();
    wxEntryCleanup();
   
    return 1;
}

int main(int argc, char* argv[]){
    //setup pixel data
    int size = 1000;
    int* pixeldata = (int*)allocate_sharedmemory(size*sizeof(int));
    for(int i=0;i<size;i++) pixeldata[i] = i % 500;
    
    //setup viewinfo
    ViewInfo* vi = (ViewInfo*)allocate_sharedmemory(sizeof(ViewInfo));
    vi->dim[0] = 100;
    vi->dim[1] = 100;
    vi->dim[2] = 1;
    strcpy(vi->dataname,get_sharedmemoryname((void*)pixeldata));
    
    //start the process sending it viewinfo
    int pid = launchprocess(testview,get_sharedmemoryname((void*)vi));
    
    //wait for process to finish    
    waitforprocess(pid);
    
    delete_sharedmemory((void*)vi);
    
    //printf("done: %lld\n",(long long)testview);
	return 0;
}
