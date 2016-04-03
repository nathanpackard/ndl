// Copyright (C) 2009   Nathan Packard   <nathanpackard@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as 
// published by the Free Software Foundation; either version 3 of the 
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public 
// License along with this program; if not, see 
// <http://www.gnu.org/licenses/>.

#include <wx/wx.h>
#include <wx/rawbmp.h>
#include <wx/dcbuffer.h>
#include <wx/spinctrl.h>
#include <wx/sysopt.h>
#include <wx/config.h> //(to let wxWidgets choose a wxConfig class for your platform)
#include <wx/textctrl.h>
#include <wx/progdlg.h>
#include <wx/docview.h>
#include <wx/filefn.h>
    
#include <hlib/helpers/colors.h>
#include <time.h>
#include <string>
#include <list>
#include <sstream>

namespace ndl {

#define TIMER_RATE 100 //refresh rate for the timer that syncs with master process
    
//*************************************************************
// foward declarations and Helper Functions
//*************************************************************
template<class VoxelType,int DIM> class NDL_MyFrame;
    
void stylize(wxWindow* win){
    win->SetBackgroundColour(*wxLIGHT_GREY);
    win->SetForegroundColour(*wxBLACK);
}
void styleizetext(wxTextCtrl* txt){
    stylize(txt);
    wxTextAttr txtstyle = txt->GetDefaultStyle();
    txtstyle.SetBackgroundColour(*wxBLACK);
    txt->SetDefaultStyle(txtstyle);
}

//*************************************************************
// Main ScrollArea Class for Displaying the Image
//*************************************************************
template<class VoxelType,int DIM>
class NDL_ScrollArea : public wxScrolledWindow {
public:
    ViewInfo<VoxelType,DIM>* vi;
    wxBitmap bitmap;
    Image<unsigned char,2>* displayimagep;
    NDL_MyFrame<VoxelType,DIM>* myparent;
    double savedwindow,savedlevel;
    wxMenu contextmenu;
    enum { ID_CommandTimer=10000 };


    #define ID_SOMETHING		2001
    #define ID_SOMETHING_ELSE	2002
    #define ID_TOGGLE_CROSSHEIR	2003

    void OnPopupClick(wxCommandEvent &evt){
        void *data=static_cast<wxMenu *>(evt.GetEventObject())->GetClientData();
        switch(evt.GetId()) {
            case ID_TOGGLE_CROSSHEIR:
                vi->showcrossheirs=!vi->showcrossheirs;
                vi->refresh();
                break;
            case ID_SOMETHING:
                wxMessageBox( _("DO SOMETHING"), _("About NVol"),wxOK | wxICON_INFORMATION, this);
                break;
            case ID_SOMETHING_ELSE:
                wxMessageBox( _("DO SOMETHING ELSE"), _("About NVol"),wxOK | wxICON_INFORMATION, this);
                break;
        }
    }

    
    NDL_ScrollArea(NDL_MyFrame<VoxelType,DIM>* parent, wxWindowID id, ViewInfo<VoxelType,DIM>* viewinfo) : wxScrolledWindow(parent, id){
        //remember the viewinfo structure
        vi = viewinfo;
        displayimagep = 0;
        myparent = parent;
        
        Connect(ID_CommandTimer,wxEVT_TIMER, wxTimerEventHandler(NDL_ScrollArea::OnTimer));
        Connect(wxID_ANY,wxEVT_PAINT,wxPaintEventHandler(NDL_ScrollArea::OnPaint));
        Connect(wxID_ANY,wxEVT_ERASE_BACKGROUND,wxPaintEventHandler(NDL_ScrollArea::OnEraseBackground));
        Connect(wxID_ANY,wxEVT_LEFT_DOWN,wxMouseEventHandler(NDL_ScrollArea::OnMouseButton));
        Connect(wxID_ANY,wxEVT_LEFT_UP,wxMouseEventHandler(NDL_ScrollArea::OnMouseButton));
        Connect(wxID_ANY,wxEVT_MIDDLE_DOWN,wxMouseEventHandler(NDL_ScrollArea::OnMouseButton));
        Connect(wxID_ANY,wxEVT_MIDDLE_UP,wxMouseEventHandler(NDL_ScrollArea::OnMouseButton));
        Connect(wxID_ANY,wxEVT_RIGHT_DOWN,wxMouseEventHandler(NDL_ScrollArea::OnMouseButton));
        Connect(wxID_ANY,wxEVT_RIGHT_UP,wxMouseEventHandler(NDL_ScrollArea::OnMouseButton));
        Connect(wxID_ANY,wxEVT_MOTION,wxMouseEventHandler(NDL_ScrollArea::OnMouseMove));
        Connect(wxID_ANY,wxEVT_MOUSEWHEEL,wxMouseEventHandler(NDL_ScrollArea::OnMouseWheel));
        
        //setup context menu handler
        contextmenu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(NDL_ScrollArea::OnPopupClick), NULL, this);

        //setup timer to recieve commands from other process
        wxTimer* timer = new wxTimer(this, ID_CommandTimer);
        timer->Start(TIMER_RATE);
    }
    ~NDL_ScrollArea(){
        //cleanup image list
        for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
            close_sharedmemory((void*)i->imagepointer->m_data);
            delete i->imagepointer;
        }
        delete displayimagep;
    }
    
    //transfer displayimage data to the native screen bitmap
    void DisplayToBitmap(Image<unsigned char,2>& displayimage){
        wxNativePixelData rawdata(bitmap);
        wxNativePixelData::Iterator p(rawdata);
        int width = bitmap.GetWidth();
        int height = bitmap.GetHeight();
        int count=0;
        for ( int y = 0; y < height; ++y ){
            wxNativePixelData::Iterator rowStart = p;
            for ( int x = 0; x < width; ++x ){
                p.Red() = displayimage(count++);
                p.Green() = displayimage(count++);
                p.Blue() = displayimage(count++);
                ++p;
            }
            p = rowStart;
            p.OffsetY(rawdata, 1);
        }
    }
    
    //set the active renderinfo
    void updateactiverender(int x,int y){
        for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
            if (x>=i->xpos && x < i->width+i->xpos && y>=i->ypos && y < i->height+i->ypos){
                vi->activerenderinfopointer = &(*i);
            }
        }
    }

    void OnMouseWheel(wxMouseEvent& event){
        //save mouse state
        int mousex,mousey;
        CalcUnscrolledPosition(event.GetX(),event.GetY(),&mousex,&mousey);
        updateactiverender(mousex,mousey);
        
        //save values
        vi->mousex=mousex;
        vi->mousey=mousey;
        vi->leftbutton=event.LeftIsDown();
        vi->middlebutton=event.MiddleIsDown();
        vi->rightbutton=event.RightIsDown();
        
        //handle the event
        vi->onmousewheel(event.GetWheelRotation()/event.GetWheelDelta());
        Refresh();
    }

    void updatestatusbar(){
        int selectionpoint[DIM];
        for(int n=0;n<DIM;n++){
            selectionpoint[n]=vi->selectioninfo.point2[n];//(std::max)(0,(std::min)(vi->selectioninfo.point2[n],vi->activerenderinfopointer->regionsize[n]-1));
        }
        if (vi->leftbutton && vi->activerenderinfopointer){
            wxString statustext = "Current Point (";
            for(int n=0;n<DIM;n++){
                if (n>0) statustext+=",";
                statustext+=wxString::Format("%d", selectionpoint[n]);
            }
            for(int c=0;c<vi->activerenderinfopointer->imagepointer->m_numcolors;c++){
                if (c==0) statustext+="): [";
                else statustext+=",";
                VoxelType& data = (*vi->activerenderinfopointer->imagepointer)(selectionpoint);
                float value = (float)(&data)[c];
                std::ostringstream os;
                os << value;
                std::string t = os.str();
                statustext+=t.c_str();
            }
            statustext+="]";
            myparent->SetStatusText( statustext );
        }
    }
    
    void OnMouseMove(wxMouseEvent& event){
        //save mouse state
        int mousex,mousey;
        CalcUnscrolledPosition(event.GetX(),event.GetY(),&mousex,&mousey);
        //updateactiverender(mousex,mousey);
        
        //save values
        vi->mousex=mousex;
        vi->mousey=mousey;
        vi->leftbutton=event.LeftIsDown();
        vi->middlebutton=event.MiddleIsDown();
        vi->rightbutton=event.RightIsDown();

        //handle the event
        vi->onmousemove();
        updatestatusbar();
        Refresh();
    }

    void OnMouseButton(wxMouseEvent& event){
        //save mouse state
        int mousex,mousey;
        CalcUnscrolledPosition(event.GetX(),event.GetY(),&mousex,&mousey);
        if (event.ButtonDown()) updateactiverender(mousex,mousey);
        
        //save values
        vi->mousex=mousex;
        vi->mousey=mousey;
        vi->leftbutton=event.LeftIsDown();
        vi->middlebutton=event.MiddleIsDown();
        vi->rightbutton=event.RightIsDown();
        
        //save the click position
        if (event.LeftDown()){
            vi->leftmousedownx=mousex;
            vi->leftmousedowny=mousey;
        }
        if (event.RightDown()){
            vi->rightmousedownx=mousex;
            vi->rightmousedowny=mousey;
        }
        if (event.MiddleDown()){
            vi->middlemousedownx=mousex;
            vi->middlemousedowny=mousey;
        }
        
        //if down and up is in same spot, do a context menu
        if (event.RightUp()){
            if (vi->rightmousedownx==mousex && vi->rightmousedowny==mousey){
                PopupMenu(&contextmenu);
            }
        }
        
        //handle the event
        if (event.LeftDown() || event.LeftUp()) vi->onmousebutton(0);
        else if (event.MiddleDown() || event.MiddleUp()) vi->onmousebutton(1);
        else if (event.RightDown() || event.RightUp()) vi->onmousebutton(2);
        vi->onmousemove();
        updatestatusbar();
        
        //handle the event
        Refresh();
        event.Skip(); //do we want this??
    }
    
    void OnEraseBackground(wxPaintEvent& event){}
    void OnPaint(wxPaintEvent& WXUNUSED(event)){
        //go through each renderinfo and do the render if needs be
        if (displayimagep){
            for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
                if (i->needsrefresh){
                    vi->renderimage(*i,*displayimagep);
                    vi->renderannotation(*i,*displayimagep);
                    vi->renderselection(*i,*displayimagep);
                    i->needsrefresh=false;
                }
            }
            DisplayToBitmap(*displayimagep);
        }
        
        //display the rendered image onto the screen
        wxAutoBufferedPaintDC dc( this );
        DoPrepareDC(dc);
        dc.DrawBitmap(bitmap, 0, 0, false);
    }
    void OnTimer(wxTimerEvent& event){
        wxresetcontextmenu<VoxelType,DIM>();

        //resize
        if (vi->resizecommand){
            //~ printf("RESIZE COMMAND RECIEVED\n");
            bitmap.Create(vi->totalwidth,vi->totalheight,24);
            SetScrollbars(1,1, vi->totalwidth, vi->totalheight, 0, 0);
            
            //RESIZE THE FRAME (MAY BE SLIGHTLY WRONG, BUT IT WORKS FOR NOW)
            int w1,h1,w2,h2,w3,h3;
            myparent->GetSize(&w1,&h1);
            wxPoint p = myparent->GetClientAreaOrigin();
            GetSize(&w2,&h2);
            myparent->SetSize(-1,-1,p.x+w1-w2+vi->totalwidth, p.y+h1-h2+vi->totalheight); 
            
            //myparent->ShowFullScreen(true);
            //wxDisplay::GetFromWindow //see this for multiple display info
                        
            //(re)create the displayimage
            delete displayimagep;
            displayimagep = new Image<unsigned char,2>(vi->totalwidth,vi->totalheight,3);
            displayimagep->setvalue(0);
            
            vi->resizecommand = false;
        }
        
        if (vi->movecommand){
            //~ printf("MOVE COMMAND RECIEVED\n");
            int w1,h1;
            myparent->GetSize(&w1,&h1);
            myparent->SetSize(vi->xpos,vi->ypos,w1,h1); 
           
            vi->movecommand = false;
        }        
        
        //add render to view
        if (vi->addrendertoviewcommand){
            //~ printf("ADDRENDERTOVIEW COMMAND RECIEVED\n");
            //setup an image
            VoxelType* data = (VoxelType*)open_sharedmemory(vi->renderinfo.dataname,vi->renderinfo.nvoxels*sizeof(VoxelType));
            Image<VoxelType,DIM>* theImage = new Image<VoxelType,DIM>(vi->renderinfo.dim,vi->renderinfo.ncolors,data);
            theImage->select(vi->renderinfo.orgin,vi->renderinfo.regionsize);
            theImage->m_min=vi->renderinfo.min;
            theImage->m_max=vi->renderinfo.max;
            for(int i=0;i<DIM;i++){
                theImage->m_voxelsize[i] = vi->renderinfo.voxelsize[i];
                theImage->m_units[i] = vi->renderinfo.units[i];
            }
                
            //setup renderview and add it to the rederinfolist
            vi->renderinfo.imagepointer = theImage;
            vi->renderinfo.needsrefresh = true;
            vi->renderinfolistpointer->push_back(vi->renderinfo);
            vi->activerenderinfopointer = &vi->renderinfo;
            myparent->OnWindowLevelText(wxCommandEvent());
            vi->addrendertoviewcommand = false;
        }
        
        //selection command (NOT DONE YET)
        if (vi->selectioncommand){
            //...
            vi->selectioncommand = false;
        }
        
        //refresh
        if (vi->refreshcommand){
            // printf("REFRESH COMMAND RECIEVED\n");
            wxString newtitle = wxString::Format(wxT("NVol - %s"), vi->filename);
            if (myparent->GetTitle()!=newtitle) myparent->SetTitle(newtitle);
            for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
                i->needsrefresh=true;
            }
            Refresh();
            vi->refreshcommand = false;
        }
        if (vi->updateLUTcommand){
            // printf("UPDATE LUT COMMAND RECIEVED\n");
            for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
                i->needsrefresh=true;
                if (vi->renderinfo.LUTflag!=i->LUTflag){
                    i->LUTflag=vi->renderinfo.LUTflag;
                    if (i->LUTflag) memcpy(i->LUT,vi->renderinfo.LUT,sizeof(i->LUT));
                }
            }
            Refresh();
            vi->updateLUTcommand = false;
        }
        //close
        if (vi->closecommand){
            //~ printf("CLOSE COMMAND RECIEVED\n");
            myparent->Close(TRUE);
            vi->closecommand = false;
        }
    }
};

//*************************************************************
// This is the main frame for the window. It contains all the controls, etc
//*************************************************************
template<class VoxelType,int DIM>
class NDL_MyFrame: public wxFrame{
    public:
    enum { ID_SnapShot = 1, ID_About, ID_COMBO, ID_SPIN, ID_AVE, ID_MIP, ID_WINDOWTEXT, ID_LEVELTEXT, ID_STATIC1, ID_STATIC2, ID_STATIC3, ID_POINT_SELECT2, ID_LINE_SELECT2, ID_BOX_SELECT2, ID_SPHERE_SELECT2 };
    ViewInfo<VoxelType,DIM>* vi;
    NDL_ScrollArea<VoxelType,DIM>* myimage;
    wxSpinCtrl *thicknessspin;
    wxColour* backgroundcolor;
    wxColour* foregroundcolor;
    wxTextCtrl* windowtextctrl;
    wxTextCtrl* leveltextctrl;
    wxComboBox* LUTcombo;
    
    NDL_MyFrame(ViewInfo<VoxelType,DIM>* viewinfo,const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame( NULL, -1, title, pos, size )
    {
        wxSystemOptions::SetOption(wxT("msw.remap"), 0);
        stylize(this);

        //save viewinfo
        vi = viewinfo;
        
        //setup toolbar
        wxToolBar* toolBar = CreateToolBar();
        stylize(toolBar);
        
        //add toolbar buttons
        wxButton* snapshotbutton = new wxButton(toolBar,ID_SnapShot, "Snapshot"); stylize(snapshotbutton); toolBar->AddControl( snapshotbutton );
        Connect(ID_SnapShot,wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(NDL_MyFrame::OnSnapShot));
        
        //tool selection
        toolBar->AddSeparator();
        
        //~ wxCheckBox* PointerCheck = new wxCheckBox(toolBar,wxID_ANY, "Pointer", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        //~ stylize(PointerCheck); toolBar->AddControl( PointerCheck );
        
        
        wxRadioButton* PointerButton = new wxRadioButton(toolBar,ID_POINT_SELECT2, "Pointer", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        wxRadioButton* LineButton = new wxRadioButton(toolBar,ID_LINE_SELECT2, "Line");
        wxRadioButton* BoxButton = new wxRadioButton(toolBar,ID_BOX_SELECT2, "Box");
        wxRadioButton* SphereButton = new wxRadioButton(toolBar,ID_SPHERE_SELECT2, "Sphere");
        stylize(PointerButton); toolBar->AddControl( PointerButton );
        stylize(LineButton); toolBar->AddControl( LineButton );
        stylize(BoxButton); toolBar->AddControl( BoxButton );
        stylize(SphereButton); toolBar->AddControl( SphereButton );
        switch(viewinfo->tooltype){
            case NDL_POINT_SELECT: { PointerButton->SetValue(true); break;}
            case NDL_LINE_SELECT: { LineButton->SetValue(true); break;}
            case NDL_BOX_SELECT: { BoxButton->SetValue(true); break;}
            case NDL_SPHERE_SELECT: { SphereButton->SetValue(true); break;}
        }
        Connect(ID_POINT_SELECT2,wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(NDL_MyFrame::OnTool));
        Connect(ID_LINE_SELECT2,wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(NDL_MyFrame::OnTool));
        Connect(ID_BOX_SELECT2,wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(NDL_MyFrame::OnTool));
        Connect(ID_SPHERE_SELECT2,wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(NDL_MyFrame::OnTool));
        Connect(wxID_ANY, wxEVT_CLOSE_WINDOW, wxCloseEventHandler(NDL_MyFrame::OnQuit));
        
        //ave/MIP
        if (DIM>2){
            toolBar->AddSeparator();
            wxRadioButton* AveButton = new wxRadioButton(toolBar,ID_AVE, "Ave", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
            wxRadioButton* MIPButton = new wxRadioButton(toolBar,ID_AVE, "MIP");
            stylize(AveButton); toolBar->AddControl( AveButton );
            stylize(MIPButton); toolBar->AddControl( MIPButton );
            toolBar->AddSeparator();
            AveButton->SetValue(true);
            Connect(ID_AVE,wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(NDL_MyFrame::OnAVE));
            Connect(ID_MIP,wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(NDL_MyFrame::OnMIP));
        
            //add thickness spinbox to toolbar
            wxStaticText* thicknesstext = new wxStaticText(toolBar,wxID_ANY," thickness:");
            thicknessspin = new wxSpinCtrl( toolBar, ID_SPIN, wxT("1"), wxDefaultPosition, wxSize(80,wxDefaultCoord), wxSP_ARROW_KEYS, 1, 100, 1 );
            stylize(thicknesstext); toolBar->AddControl( thicknesstext );
            stylize(thicknessspin); toolBar->AddControl( thicknessspin );
            Connect(ID_SPIN,wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(NDL_MyFrame::OnThicknessSpin));
        }
        
        if (vi->LUTcontrols){
            toolBar->AddSeparator();
            //add LUT dropdown
            setupcolormaps(); //setup LUTs
            const wxString choices[] = {_("no colormap"),_("coldhot"),_("hot"),_("red"),_("green"),_("blue")};
            LUTcombo = new wxComboBox(toolBar,ID_COMBO,choices[0],wxDefaultPosition,wxDefaultSize,6,choices,wxCB_READONLY);
            stylize(LUTcombo); toolBar->AddControl( LUTcombo );
            Connect(ID_COMBO,wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler(NDL_MyFrame::OnLUTCombo));
        }
        
        //add window / level text controls
        if (vi->winlevelcontrols) toolBar->AddSeparator();
        wxStaticText* windowtext = new wxStaticText(toolBar,wxID_ANY," window:"); 
        stylize(windowtext); toolBar->AddControl( windowtext );
        windowtextctrl = new wxTextCtrl( toolBar, ID_WINDOWTEXT, "" ); styleizetext(windowtextctrl); 
        stylize(windowtextctrl); toolBar->AddControl( windowtextctrl );
        wxStaticText* leveltext = new wxStaticText(toolBar,wxID_ANY," level:"); 
        stylize(leveltext); toolBar->AddControl( leveltext );
        leveltextctrl = new wxTextCtrl( toolBar, ID_LEVELTEXT, "" ); styleizetext(leveltextctrl); 
        stylize(leveltextctrl); toolBar->AddControl( leveltextctrl );
        Connect(ID_LEVELTEXT,wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(NDL_MyFrame::OnWindowLevelText));
        Connect(ID_WINDOWTEXT,wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(NDL_MyFrame::OnWindowLevelText));
        if (!vi->winlevelcontrols){
            windowtext->Hide();
            windowtextctrl->Hide();
            leveltext->Hide();
            leveltextctrl->Hide();
        }
        
        //build the toolbar
        if (vi->showcontrols) toolBar->Realize();
        
        //setup statusbar
        CreateStatusBar();
        
        //setup scroll area to display the image
        myimage = new NDL_ScrollArea<VoxelType,DIM>(this, wxID_ANY, viewinfo );
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(myimage, 1, wxALL | wxEXPAND);
        SetSizer(sizer);
        
        //setup icon
	if (wxFileExists("nvol.ico")) SetIcon(wxIcon("nvol.ico",wxBITMAP_TYPE_ICO));
	//~ if (wxFileExists("nvol.ico")) 
        //~ SetIcon(wxIcon("nvol.ico",wxBITMAP_TYPE_ICO_RESOURCE));
        
    }
    void OnSnapShot(wxCommandEvent& WXUNUSED(event)){
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
        wxFileDialog *SaveDialog = new wxFileDialog(this, _("Save Snapshot As"), wxEmptyString, wxEmptyString, filefilter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);
        //~ SaveDialog->SetFilterIndex(myimage->displayimagep->m_imagepluginindex);
        if (SaveDialog->ShowModal() == wxID_OK){
            wxString CurrentDocPath = SaveDialog->GetPath();
            std::string paramstring = "-filetype:";
            paramstring+=imagetypes[SaveDialog->GetFilterIndex()];
            
            //do the save
            if (vi->activerenderinfopointer){
                myimage->displayimagep->savefile(CurrentDocPath.c_str(),paramstring,false);
            }
        }
    }
    void OnAVE(wxCommandEvent& event){
        for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
            i->displaymode = NDL_AVE;
            i->needsrefresh=true;
        }
        myimage->Refresh();
    }
    void OnMIP(wxCommandEvent& event){
        for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
            i->displaymode = NDL_MIP;
            i->needsrefresh=true;
        }
        myimage->Refresh();
    }
    void OnThicknessSpin(wxCommandEvent& event){
        for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
            i->axis3thickness = thicknessspin->GetValue();
            i->needsrefresh=true;
        }
        myimage->Refresh();
    }
    
    void OnQuit(wxCloseEvent& event){
        if (vi->closeevent){
            if (vi->closeevent(vi) || !event.CanVeto()){ Destroy(); }
            else event.Veto();
        } else Destroy();
    }
    
    void OnTool(wxCommandEvent& event){
        int tool = event.GetId();
        switch (tool){
            case ID_POINT_SELECT2: { vi->tooltype=NDL_POINT_SELECT; break;}
            case ID_LINE_SELECT2: { vi->tooltype=NDL_LINE_SELECT; break;}
            case ID_BOX_SELECT2: { vi->tooltype=NDL_BOX_SELECT; break;}
            case ID_SPHERE_SELECT2: { vi->tooltype=NDL_SPHERE_SELECT; break;}
        }
    }
    
    void getwindowlevel(double* windowp,double* levelp){
        //get min/max from the current image
        float min = vi->activerenderinfopointer->imagepointer->m_min;
        float max = vi->activerenderinfopointer->imagepointer->m_max;

        //get window/level values from textbox
        wxString windowtext  = windowtextctrl->GetValue();
        wxString leveltext  = leveltextctrl->GetValue();
        if(!windowtext.ToDouble(windowp) || !leveltext.ToDouble(levelp)){
            *windowp=max-min; 
            *levelp=*windowp/2 + min;
        }
    }
    
    void setwindowlevel(double window,double level){
        windowtextctrl->SetValue(wxString::Format(wxT("%f"), window));
        leveltextctrl->SetValue(wxString::Format(wxT("%f"), level));
        for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
            i->min = level - window/2;
            i->max = level + window/2;
            i->needsrefresh=true;
        }
        myimage->Refresh();
    }
    
    void OnWindowLevelText(wxCommandEvent& event){
        double window,level;
        getwindowlevel(&window,&level);
        setwindowlevel(window,level);
    }
    
    void OnLUTCombo(wxCommandEvent& event){
        //set the LUT for each render
        for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = vi->renderinfolistpointer->begin(); i != vi->renderinfolistpointer->end(); ++i){
            if (LUTcombo->GetValue()==_("no colormap")){
                i->LUTflag=false;
                i->needsrefresh=true;
            }
            if (LUTcombo->GetValue()==_("coldhot")){
                memcpy(i->LUT,coldhotcolortable,sizeof(coldhotcolortable));
                i->LUTflag=true;
                i->needsrefresh=true;
            }
            if (LUTcombo->GetValue()==_("hot")){
                memcpy(i->LUT,hotcolortable,sizeof(hotcolortable));
                i->LUTflag=true;
                i->needsrefresh=true;
            }
            if (LUTcombo->GetValue()==_("red")){
                memcpy(i->LUT,redcolortable,sizeof(redcolortable));
                i->LUTflag=true;
                i->needsrefresh=true;
            }
            if (LUTcombo->GetValue()==_("green")){
                memcpy(i->LUT,greencolortable,sizeof(greencolortable));
                i->LUTflag=true;
                i->needsrefresh=true;
            }
            if (LUTcombo->GetValue()==_("blue")){
                memcpy(i->LUT,bluecolortable,sizeof(bluecolortable));
                i->LUTflag=true;
                i->needsrefresh=true;
            }
        }
        myimage->Refresh();
    }
};

template<class VoxelType,int DIM>
class NDL_MyApp: public wxApp{
    public:
    ViewInfo<VoxelType,DIM>* theViewInfo;
    wxString theTitle;
    void setvi(ViewInfo<VoxelType,DIM>* vi){ theViewInfo = vi; }
    void settitle(wxString title){ theTitle = title; }
    bool OnInit(){
        NDL_MyFrame<VoxelType,DIM>* myframe = new NDL_MyFrame<VoxelType,DIM>( theViewInfo, theTitle, wxPoint(50, 50),wxSize(450,340) );
        myframe->Show();
        SetTopWindow(myframe);
        return true;
    } 
};

//*******************************************
//These functions are called as a new process
//*******************************************
template<class VoxelType,int DIM>
int wxmakeview(char* message){
    //get sharedmemory (viewinfo and pixeldata)
    ViewInfo<VoxelType,DIM>* vi = (ViewInfo<VoxelType,DIM>*)open_sharedmemory(message,sizeof(ViewInfo<VoxelType,DIM>));
    NDL_MyApp<VoxelType,DIM>* myapp = new NDL_MyApp<VoxelType,DIM>();
    myapp->setvi(vi);
    myapp->settitle(wxString::Format(wxT("NVol - %s"), vi->filename));
    
    //setup vars specific to this viewer process
    std::list< RenderInfo<VoxelType,DIM> > renderinfolist;
    std::list< SelectionInfo<VoxelType,DIM> > annotationlist;
    vi->renderinfolistpointer = &renderinfolist;
    vi->annotationlistpointer = &annotationlist;
        
    wxApp::SetInstance( myapp );
    int dummyargc=1;char** dummyargv=0;
    wxEntryStart( dummyargc, dummyargv );
    wxTheApp->OnInit();
    wxTheApp->OnRun();
    wxTheApp->OnExit();
    wxEntryCleanup();
    
    //close sharedmemory
    close_sharedmemory((void*)vi);
    return 0;
}

template<class VoxelType,int DIM>
void wxgetwindowlevel(double* window,double* level){
    NDL_MyFrame<VoxelType,DIM>* framepntr = static_cast<NDL_MyFrame<VoxelType,DIM>*>(wxTheApp->GetTopWindow());
    framepntr->getwindowlevel(window,level);
}

template<class VoxelType,int DIM>
void wxsetwindowlevel(double window,double level){
    NDL_MyFrame<VoxelType,DIM>* framepntr = static_cast<NDL_MyFrame<VoxelType,DIM>*>(wxTheApp->GetTopWindow());
    framepntr->setwindowlevel(window,level);
}

template<class VoxelType,int DIM>
void wxaddtocontextmenu(char* text){
    //~ NDL_MyFrame<VoxelType,DIM>* framepntr = static_cast<NDL_MyFrame<VoxelType,DIM>*>(wxTheApp->GetTopWindow());
    //~ framepntr->myimage->contextmenu.Append(ID_SOMETHING,text);
}

template<class VoxelType,int DIM>
void wxresetcontextmenu(){
    NDL_MyFrame<VoxelType,DIM>* framepntr = static_cast<NDL_MyFrame<VoxelType,DIM>*>(wxTheApp->GetTopWindow());
    //~ framepntr->myimage->contextmenu.Destroy(  id );
    if (framepntr->vi->showcrossheirs) framepntr->myimage->contextmenu.Append(ID_TOGGLE_CROSSHEIR, 	"Hide Crossheir");
    else framepntr->myimage->contextmenu.Append(ID_TOGGLE_CROSSHEIR, 	"Show Crossheir");
    framepntr->myimage->contextmenu.Append(ID_SOMETHING, 	"Do something");
    framepntr->myimage->contextmenu.Append(ID_SOMETHING_ELSE, 	"Do something else");
}

}
