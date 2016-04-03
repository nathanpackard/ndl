namespace ndl {
    
void viewND_axisfromlayoutpos(unsigned int x,unsigned int y,unsigned int& axis1,unsigned int& axis2){ unsigned int t = x<<1; if ((y & 1)==0 || t<y+1){ axis1=t; axis2=y+1; } else { axis1=t+1; axis2=y; }}
int viewND_layoutindex(unsigned int minaxis,unsigned int maxaxis){ return maxaxis*(maxaxis-1)/2 + minaxis; }

template<class VoxelType,int DIM> void viewND_setuprender(ViewInfo<VoxelType,DIM>* view, int xpos,int ypos,int width,int height,int axis1,int axis2,int axis3,int axis3pos,unsigned char* rendercolor){
    //set the values
    view->renderinfo.xpos=xpos;
    view->renderinfo.ypos=ypos;
    view->renderinfo.width=width;
    view->renderinfo.height=height;
    view->renderinfo.axis1=axis1;
    view->renderinfo.axis2=axis2;
    view->renderinfo.axis3=axis3;
    view->renderinfo.axis3pos=axis3pos;
    memcpy(view->renderinfo.rendercolor,rendercolor,3);
    //set the selection as well
    if (axis3!=-1){
        view->selectioninfo.point1[axis3] = axis3pos;
        view->selectioninfo.point2[axis3] = axis3pos;
    }
}

template<class VoxelType,int DIM>
class OrthoRenderND {
public:
    static ViewInfo<VoxelType,DIM>* openview(Image<VoxelType,DIM>& image,ViewInfo<VoxelType,DIM>* view){
        if (DIM==2){
            view->resizeview(image.m_regionsize[0],image.m_regionsize[1]);
            viewND_setuprender(view,0, 0,image.m_regionsize[0],image.m_regionsize[1],0,1,-1,-1,(unsigned char*)&randomswatch[0]);
            view->addrendertoview(image,view->renderinfo.rendertype);
            return view;
        }
        
        const int NUMX = (DIM+1)/2; //we add 1 so that the division rounds up instead of down
        const int NUMY = DIM-1;
            
        //initialize thicknessaxis and thickness to good default values
        int thicknessaxis[NUMX*NUMY+1]; //+1 makes it so we never have a zero sized array
        for(int i=0;i<DIM;i++){
        for(int j=0;j<DIM;j++){
            if (i<j){
                int index = viewND_layoutindex(i,j);
                thicknessaxis[index] = 0;
                while(thicknessaxis[index]==i || thicknessaxis[index]==j){
                    thicknessaxis[index]++;
                    if (thicknessaxis[index]>=DIM) thicknessaxis[index]=0;
                }
            }
        }}
        
        //Calculate Total Width and Height of each column and row in the layout
        //Also calculate the total width and height of the display image
        int margin = view->margin = 5;
        int margin2 = margin*2;
        int widtharray[NUMX+1]; //+1 makes it so we never have a zero sized array
        int heightarray[NUMY+1]; //+1 makes it so we never have a zero sized array
        for(unsigned int i=0;i<NUMX;i++) widtharray[i] = 0;
        for(unsigned int j=0;j<NUMY;j++) heightarray[j] = 0;
        for(unsigned int i=0;i<NUMX;i++){
        for(unsigned int j=0;j<NUMY;j++){
            unsigned int axis1,axis2;
            viewND_axisfromlayoutpos(i,j,axis1,axis2);
            if (axis1<DIM && axis2<DIM){
                widtharray[i] = (std::max)(widtharray[i],image.m_regionsize[axis1]);
                heightarray[j] = (std::max)(heightarray[j],image.m_regionsize[axis2]);
            }
        }}
        int totalwidth = 0; for(int i=0;i<NUMX;i++) totalwidth+=widtharray[i]+margin2;
        int totalheight = 0; for(int j=0;j<NUMY;j++) totalheight+=heightarray[j]+margin2;
        view->resizeview(totalwidth,totalheight);
        
        //create the renderings
        int nextcolorindex=0;
        for(unsigned int j=0;j<NUMY;j++){
        for(unsigned int i=0;i<NUMX;i++){
            unsigned int axis1,axis2;
            viewND_axisfromlayoutpos(i,j,axis1,axis2);
            if (axis1<DIM && axis2<DIM){
                //create an ortho render
                int xpos=margin;
                int ypos=margin;
                for(int ti=0;ti<i;ti++) xpos+=widtharray[ti]+margin2;
                for(int tj=0;tj<j;tj++) ypos+=heightarray[tj]+margin2;
                int axis3 = thicknessaxis[viewND_layoutindex(axis1,axis2)];
                int axis3pos = image.m_regionsize[axis3]/2;
                viewND_setuprender(view, xpos, ypos, widtharray[i],heightarray[j],axis1,axis2,axis3,axis3pos,(unsigned char*)&randomswatch[3*nextcolorindex]);
                view->addrendertoview(image,view->renderinfo.rendertype);
                nextcolorindex++;
                if (nextcolorindex>=sizeof(randomswatch)/3) nextcolorindex=0;
            }
        }}
        
        return view;
    }
    static void renderimage(ViewInfo<VoxelType,DIM>* view,RenderInfo<VoxelType,DIM>& renderinfo,Image<unsigned char,2>& displayimage){
        Image<VoxelType,DIM>& image = *renderinfo.imagepointer;
        
        //select displayimage region
        displayimage.select(renderinfo.xpos,renderinfo.ypos,renderinfo.width,renderinfo.height);
        displayimage.setvalue(0);
        
        //calculate scale (a) and bias (b)
        float Range = 255;
        float a = Range/(renderinfo.max - renderinfo.min);
        float b = Range/(1-(float)renderinfo.max/(float)renderinfo.min);
        int ncolor = image.m_numcolors;
        
        if (renderinfo.axis3==-1){
            //update the display image
            NDL_FOREACHPL(image){
                int imagecoord[DIM];
                int imageindex,colorindex;
                NDL_GETCOLORCOORD(imageindex,colorindex,imagecoord);
                
                if (colorindex>2) continue; //only process up to 3 colors
                
                //scale and bias and clamp, then update the display image
                int displayindex = (imagecoord[renderinfo.axis1]-image.m_orgin[renderinfo.axis1]+displayimage.m_orgin[0])*displayimage.m_dimPROD[0]
                                 + (imagecoord[renderinfo.axis2]-image.m_orgin[renderinfo.axis2]+displayimage.m_orgin[1])*displayimage.m_dimPROD[1];

                //calculate value
                float value=image(imageindex);                            
                int ivalue = a*value+b + 0.5;
                unsigned char tvalue = CLAMP(ivalue,0,255);
                if (renderinfo.LUTflag && ncolor==1){
                    displayimage(displayindex) = renderinfo.LUT[tvalue*3];
                    displayimage(displayindex+1) = renderinfo.LUT[tvalue*3+1];
                    displayimage(displayindex+2) = renderinfo.LUT[tvalue*3+2];
                } else {
                    displayimage(displayindex+colorindex) = tvalue;
                    if (ncolor==1){
                        displayimage(displayindex+1) = tvalue;
                        displayimage(displayindex+2) = tvalue;
                    }
                }
            } NDL_ENDFOREACH
        } else {
            int dt,th;
            int savedorgin[DIM],savedregion[DIM];
            image.saveselection(savedorgin,savedregion);
            
            //select image region
            int halfthickness = renderinfo.axis3thickness/2;
            for(int n=0;n<DIM;n++){
                if (n!=renderinfo.axis1 && n!=renderinfo.axis2){
                    if (n==renderinfo.axis3){
                        image.m_orgin[n] = renderinfo.axis3pos-halfthickness;
                        image.m_regionsize[n] = renderinfo.axis3thickness;
                    } else image.m_regionsize[n] = 1;
                }
            }
            image.select(image.m_orgin,image.m_regionsize);
                
            //render the image
            dt = image.m_dimPROD[renderinfo.axis3];
            th = image.m_regionsize[renderinfo.axis3];
            NDL_FOREACHLINEPL(image,renderinfo.axis3){
                int imagecoord[DIM];
                int imageindex,colorindex;
                NDL_GETCOLORCOORD(imageindex,colorindex,imagecoord);
                
                if (colorindex>2) continue; //only process up to 3 colors
                
                //scale and bias and clamp, then update the display image
                int displayindex = (imagecoord[renderinfo.axis1]-image.m_orgin[renderinfo.axis1]+displayimage.m_orgin[0])*displayimage.m_dimPROD[0]
                                 + (imagecoord[renderinfo.axis2]-image.m_orgin[renderinfo.axis2]+displayimage.m_orgin[1])*displayimage.m_dimPROD[1];

                //calculate value
                float value=image(imageindex);
                int th2=(std::min)((image.m_dimarray[renderinfo.axis3] - imagecoord[renderinfo.axis3])/2,th);
                if (renderinfo.displaymode==NDL_AVE){
                    for(int t=1;t<th2;++t) value+=image(imageindex+=dt);
                    value/=th;
                } else
                if (renderinfo.displaymode==NDL_MIP){
                    for(int t=1;t<th2;++t) value=MAX2(image(imageindex+=dt),value);
                }
                            
                int ivalue = a*value+b + 0.5;
                unsigned char tvalue = CLAMP(ivalue,0,255);
                if (renderinfo.LUTflag && ncolor==1){
                    displayimage(displayindex) = renderinfo.LUT[tvalue*3];
                    displayimage(displayindex+1) = renderinfo.LUT[tvalue*3+1];
                    displayimage(displayindex+2) = renderinfo.LUT[tvalue*3+2];
                } else {
                    displayimage(displayindex+colorindex) = tvalue;
                    if (ncolor==1){
                        displayimage(displayindex+1) = tvalue;
                        displayimage(displayindex+2) = tvalue;
                    }
                }
            } NDL_ENDFOREACH
            image.select(savedorgin,savedregion);
        }
    }
    static void drawcrossheir(bool drawit,ViewInfo<VoxelType,DIM>* view,RenderInfo<VoxelType,DIM>& renderinfo,Image<unsigned char,2>& displayimage,int centergapx,int centergapy){
        //draw or remove the borders
        if (view->margin>0){
            int origin[2];
            int size[2];
            origin[0] = renderinfo.xpos;
            origin[1] = renderinfo.ypos;
            size[0] = renderinfo.dim[renderinfo.axis1];
            size[1] = renderinfo.dim[renderinfo.axis2];
            origin[0]--;
            origin[1]--;
            size[0]+=2;
            size[1]+=2;
            if (drawit){
                //draw the borders
                displayimage.DrawRectangle(origin,size,renderinfo.rendercolor);
            } else {
                //erase the borders if we are not drawing crossheirs
                displayimage.DrawRectangle(origin,size,(unsigned char)0);
            }
        }
            
        if (drawit){
            //get crossheir colors
            unsigned char* color1 = renderinfo.rendercolor;
            unsigned char* color2 = renderinfo.rendercolor;
            if (renderinfo.axis3!=-1 && view->renderinfolistpointer){
                std::list< RenderInfo<VoxelType,DIM> >& renderinfolist = *view->renderinfolistpointer;
                for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = renderinfolist.begin(); i != renderinfolist.end(); ++i){
                    if (((*i).axis1==renderinfo.axis3 && (*i).axis2==renderinfo.axis2) || ((*i).axis2==renderinfo.axis3 && (*i).axis1==renderinfo.axis2) ){
                        color1 = (*i).rendercolor;
                    }
                    if (((*i).axis1==renderinfo.axis1 && (*i).axis2==renderinfo.axis3) || ((*i).axis2==renderinfo.axis1 && (*i).axis1==renderinfo.axis3) ){
                        color2 = (*i).rendercolor;
                    }
                }
            }
            
            //draw crossheirs
            int start[2];
            int end[2];
            int middle[2];
            middle[0] = (view->selectioninfo.point1[renderinfo.axis1] + view->selectioninfo.point2[renderinfo.axis1])/2;
            middle[1] = (view->selectioninfo.point1[renderinfo.axis2] + view->selectioninfo.point2[renderinfo.axis2])/2;
            
            start[0] = renderinfo.xpos + middle[0];
            end[0] = start[0];
            start[1] = renderinfo.ypos;
            end[1] = renderinfo.ypos + (std::max)((int)0,middle[1] - centergapy);
            displayimage.DrawLine(start,end,color1);
            start[1] = renderinfo.ypos + (std::min)(renderinfo.dim[renderinfo.axis2],middle[1] + centergapy);
            end[1] = renderinfo.ypos + renderinfo.dim[renderinfo.axis2];
            displayimage.DrawLine(start,end,color1);
            
            start[1] = renderinfo.ypos + middle[1];
            end[1] = start[1];
            start[0] = renderinfo.xpos;
            end[0] = renderinfo.xpos + (std::max)((int)0,middle[0] - centergapx);
            displayimage.DrawLine(start,end,color2);
            start[0] = renderinfo.xpos + (std::min)(renderinfo.dim[renderinfo.axis1],middle[0] + centergapx);
            end[0] = renderinfo.xpos + renderinfo.dim[renderinfo.axis1];
            displayimage.DrawLine(start,end,color2);
        }
    }
    
    static void drawselection(ViewInfo<VoxelType,DIM>* view,RenderInfo<VoxelType,DIM>& renderinfo,Image<unsigned char,2>& displayimage,SelectionInfo<VoxelType,DIM>& selectioninfo,bool showcrossheirs,unsigned char* color = (unsigned char*)color_yellow){
        //printf("selectioninfo.point1[renderinfo.axis3]: %d\n",selectioninfo.point1[renderinfo.axis3]);
        //printf("selectioninfo.point2[renderinfo.axis3]: %d\n",selectioninfo.point2[renderinfo.axis3]);
        //printf("renderinfo.axis3pos: %d\n",renderinfo.axis3pos);
        //display selections for this render
        switch(selectioninfo.selectiontype){
            case NDL_POINT_SELECT: {
                drawcrossheir(showcrossheirs,view,renderinfo,displayimage,20,20);
                break;
            }
            case NDL_LINE_SELECT: {
                int start[2];
                int end[2];
                int middle[2];
                start[0] = renderinfo.xpos + selectioninfo.point1[renderinfo.axis1];
                start[1] = renderinfo.ypos + selectioninfo.point1[renderinfo.axis2];
                end[0] = renderinfo.xpos + selectioninfo.point2[renderinfo.axis1];
                end[1] = renderinfo.ypos + selectioninfo.point2[renderinfo.axis2];
                middle[0] = (start[0]+end[0]) / 2;
                middle[1] = (start[1]+end[1]) / 2;
                float dx = end[0]-start[0];
                float dy = end[1]-start[1];
                float dist = sqrt(dx*dx + dy*dy);
                if (renderinfo.axis3==-1 || (renderinfo.axis3pos==selectioninfo.point1[renderinfo.axis3] && renderinfo.axis3pos==selectioninfo.point2[renderinfo.axis3])){
                    displayimage.DrawLine(start,end,color);
                    char text[20];
                    if (strcmp(renderinfo.units[renderinfo.axis1],renderinfo.units[renderinfo.axis2])==0){
                        sprintf(text,"%.02f%s",dist,renderinfo.units[renderinfo.axis1]);
                    } else {
                        sprintf(text,"%.02f%s-%s",dist,renderinfo.units[renderinfo.axis1],renderinfo.units[renderinfo.axis2]);
                    }
                    displayimage.DrawText(middle,text,color);
                }
                drawcrossheir(showcrossheirs,view,renderinfo,displayimage,20+abs(dx)/2,20+abs(dy)/2);
                break;
            }
            case NDL_BOX_SELECT: {
                int origin[2];
                int size[2];
                int minx = (std::min)(selectioninfo.point1[renderinfo.axis1],selectioninfo.point2[renderinfo.axis1]);
                int miny = (std::min)(selectioninfo.point1[renderinfo.axis2],selectioninfo.point2[renderinfo.axis2]);
                int maxx = (std::max)(selectioninfo.point1[renderinfo.axis1],selectioninfo.point2[renderinfo.axis1]);
                int maxy = (std::max)(selectioninfo.point1[renderinfo.axis2],selectioninfo.point2[renderinfo.axis2]);
                origin[0] = renderinfo.xpos + minx;
                origin[1] = renderinfo.ypos + miny;
                size[0] = maxx - minx + 1;
                size[1] = maxy - miny + 1;
                if (renderinfo.axis3==-1 || ((renderinfo.axis3pos>=selectioninfo.point1[renderinfo.axis3] && renderinfo.axis3pos<=selectioninfo.point2[renderinfo.axis3]) || (renderinfo.axis3pos<=selectioninfo.point1[renderinfo.axis3] && renderinfo.axis3pos>=selectioninfo.point2[renderinfo.axis3]))){
                    displayimage.DrawRectangle(origin,size,color);
                }
                drawcrossheir(showcrossheirs,view,renderinfo,displayimage,20+(maxx-minx)/2,20+(maxy-miny)/2);
                break;
            }
            case NDL_SPHERE_SELECT: {
                int center[2];
                center[0] = (selectioninfo.point1[renderinfo.axis1] + selectioninfo.point2[renderinfo.axis1])/2;
                center[1] = (selectioninfo.point1[renderinfo.axis2] + selectioninfo.point2[renderinfo.axis2])/2;
                int diameter = 2*getradius<int,DIM>(selectioninfo.point1,selectioninfo.point2);
                if (renderinfo.axis3!=-1){
                    int center3 = (selectioninfo.point1[renderinfo.axis3] + selectioninfo.point2[renderinfo.axis3])/2;
                    diameter = radiusfromnsphere(renderinfo.axis3pos - center3,diameter/2.0)*2;
                }
                int origin[2];
                int size[2];
                origin[0] = renderinfo.xpos + center[0] - diameter/2;
                origin[1] = renderinfo.ypos + center[1] - diameter/2;
                size[0] = diameter;
                size[1] = diameter;
                if (diameter) displayimage.DrawEllipse(origin,size,color);
                drawcrossheir(showcrossheirs,view,renderinfo,displayimage,20+diameter/2,20+diameter/2);
                break;
            }
        }
    }
    
    static void renderselection(ViewInfo<VoxelType,DIM>* view,RenderInfo<VoxelType,DIM>& renderinfo,Image<unsigned char,2>& displayimage){
        drawselection(view,renderinfo,displayimage,view->selectioninfo,view->showcrossheirs);
    }
    static void renderannotation(ViewInfo<VoxelType,DIM>* view,RenderInfo<VoxelType,DIM>& renderinfo,Image<unsigned char,2>& displayimage){
        if (!view->annotationlistpointer) return;
        std::list< SelectionInfo<VoxelType,DIM> >& annotationlist = *view->annotationlistpointer;
        for(std::list< SelectionInfo<VoxelType,DIM> >::iterator i = annotationlist.begin(); i != annotationlist.end(); ++i){
            drawselection(view,renderinfo,displayimage,*i,false,(unsigned char*)color_darkred);
        }
    }
    
    static void syncviews(ViewInfo<VoxelType,DIM>* view,int point[DIM]){
        if (!view->renderinfolistpointer) return;
        std::list< RenderInfo<VoxelType,DIM> >& renderinfolist = *view->renderinfolistpointer;
        for(std::list< RenderInfo<VoxelType,DIM> >::iterator i = renderinfolist.begin(); i != renderinfolist.end(); ++i){
            if (&(*i)!=view->activerenderinfopointer){
                if (i->axis3==view->activerenderinfopointer->axis1){
                    i->axis3pos = point[i->axis3];
                    i->needsrefresh=true;
                }
                if (i->axis3==view->activerenderinfopointer->axis2){
                    i->axis3pos = point[i->axis3];
                    i->needsrefresh=true;
                }
            }
        }
    }
    
    static void clickselection(ViewInfo<VoxelType,DIM>* view,SelectionInfo<VoxelType,DIM>& currselection){
        int axis1 = view->activerenderinfopointer->axis1;
        int axis2 = view->activerenderinfopointer->axis2;
        int axis3 = view->activerenderinfopointer->axis3;
        bool hasdepth = (axis3!=-1);
        int pos1=(std::max)(0,(std::min)((int)(view->mousex - view->activerenderinfopointer->xpos),(int)(view->activerenderinfopointer->regionsize[axis1]-1)));
        int pos2=(std::max)(0,(std::min)((int)(view->mousey - view->activerenderinfopointer->ypos),(int)(view->activerenderinfopointer->regionsize[axis2]-1)));
        int currpoint[2];
        int point1[2];
        int point2[2];
        currpoint[0]=pos1;
        currpoint[1]=pos2;
        point1[0]=currselection.point1[axis1];
        point1[1]=currselection.point1[axis2];
        point2[0]=currselection.point2[axis1];
        point2[1]=currselection.point2[axis2];
        
        int pos3,halfthickness,pos3_lower,pos3_upper;
        if (hasdepth){
            pos3 = view->activerenderinfopointer->axis3pos;
            halfthickness = view->activerenderinfopointer->axis3thickness/2;
            pos3_lower = (std::max)(0,pos3-halfthickness);
            pos3_upper = (std::min)(view->activerenderinfopointer->regionsize[axis3]-1,pos3_lower+view->activerenderinfopointer->axis3thickness-1);
            pos3_lower=(std::max)(0,pos3_upper-view->activerenderinfopointer->axis3thickness+1);
        }
        
        switch(currselection.selectiontype){
            case NDL_POINT_SELECT: {
                {
                    //printf("new point\n");
                    //Not an existing selection
                    currselection.point1[axis1] = currpoint[0];
                    currselection.point1[axis2] = currpoint[1];
                    if (hasdepth) currselection.point1[axis3] = pos3;
                    currselection.point2[axis1] = currpoint[0];
                    currselection.point2[axis2] = currpoint[1];
                    if (hasdepth) currselection.point2[axis3] = pos3;
                    currselection.selectiontype=view->tooltype;
                    currselection.selectionstate=NDL_SELECTIONNEW;
                }
                syncviews(view,currselection.point1);
                view->activerenderinfopointer->needsrefresh=true;
                break;
            }
            case NDL_LINE_SELECT: {
                bool inrangeflag = (!hasdepth || (pos3==currselection.point1[axis3] && pos3==currselection.point2[axis3]));
                if (inrangeflag && getlineendpoint<int,2>(currpoint,point1,point2)){
                    //printf("line endpoint\n");
                    //if we grabbed an endpoint to an existing selection
                    currselection.point1[axis1] = point1[0];
                    currselection.point1[axis2] = point1[1];
                    currselection.point2[axis1] = point2[0];
                    currselection.point2[axis2] = point2[1];
                    currselection.selectionstate=NDL_SELECTIONNEW;
                } else
                if (inrangeflag && pointonline<int,2>(currpoint,point1,point2)){
                    //printf("line grabbed\n");
                    //if we grabbed an existing line selection
                    currselection.selectionstate=NDL_SELECTIONMOVE;
                } else {
                     //printf("new line\n");
                   //Not an existing selection
                    currselection.point1[axis1] = currpoint[0];
                    currselection.point1[axis2] = currpoint[1];
                    if (hasdepth) currselection.point1[axis3] = pos3;
                    currselection.point2[axis1] = currpoint[0];
                    currselection.point2[axis2] = currpoint[1];
                    if (hasdepth) currselection.point2[axis3] = pos3;
                    currselection.selectiontype=view->tooltype;
                    currselection.selectionstate=NDL_SELECTIONNEW;
                }
                int centerpoint[DIM];
                for(int i=0;i<DIM;i++) centerpoint[i] = (currselection.point1[i]+currselection.point2[i])/2;
                syncviews(view,centerpoint);
                view->activerenderinfopointer->needsrefresh=true;
                break;
            }
            case NDL_BOX_SELECT: {
                //get the corners from the box
                int tempdim,tempside;
                bool inrangeflag = (!hasdepth || (pos3>=currselection.point1[axis3] && pos3<=currselection.point2[axis3]) || (pos3<=currselection.point1[axis3] && pos3>=currselection.point2[axis3]));
                if (inrangeflag && getcubecorner<int,2>(currpoint,point1,point2)){
                    //printf("box corner\n");
                    //if we grabbed a corner of an existing selection
                    currselection.point1[axis1] = point1[0];
                    currselection.point1[axis2] = point1[1];
                    currselection.point2[axis1] = point2[0];
                    currselection.point2[axis2] = point2[1];
                    currselection.selectionstate=NDL_SELECTIONNEW;
                } else
                if (inrangeflag && getcubesideindex<int,2>(currpoint,point1,point2,&tempdim,&tempside)){
                    if (tempdim==0) currselection.selectionactivedimension = axis1;
                    if (tempdim==1) currselection.selectionactivedimension = axis2;
                    currselection.selectionactiveside = tempside;
                    //printf("box edge\n");
                    //if we grabbed the edge of an existing box selection
                    currselection.selectionstate=NDL_SELECTIONMOVEEDGE;
                } else
                if (inrangeflag && pointincube<int,2>(currpoint,point1,point2)){
                    //printf("box grabbed\n");
                    //if we grabbed an existing box selection
                    currselection.selectionstate=NDL_SELECTIONMOVE;
                } else {
                    //printf("new box\n");
                    //Not an existing selection
                    currselection.point1[axis1] = currpoint[0];
                    currselection.point1[axis2] = currpoint[1];
                    if (hasdepth) currselection.point1[axis3] = pos3;
                    currselection.point2[axis1] = currpoint[0];
                    currselection.point2[axis2] = currpoint[1];
                    if (hasdepth) currselection.point2[axis3] = pos3;
                    currselection.selectiontype=view->tooltype;
                    currselection.selectionstate=NDL_SELECTIONNEW;
                }
                int centerpoint[DIM];
                for(int i=0;i<DIM;i++) centerpoint[i] = (currselection.point1[i]+currselection.point2[i])/2;
                syncviews(view,centerpoint);
                view->activerenderinfopointer->needsrefresh=true;
                break;
            }
            case NDL_SPHERE_SELECT: {
                int centerpoint[DIM];
                for(int i=0;i<DIM;i++) centerpoint[i] = (currselection.point1[i]+currselection.point2[i])/2;
                double radius=getradius<int,DIM>(currselection.point1,currselection.point2);
                double visibleradius = radius;
                if (hasdepth) visibleradius = radiusfromnsphere(pos3 - centerpoint[axis3],radius);
                double ratio = visibleradius/radius;
                
                int center[2];
                center[0] = centerpoint[axis1];
                center[1] = centerpoint[axis2];
                
                bool inrangeflag = (!hasdepth || (pos3>=centerpoint[axis3]-radius && pos3<=centerpoint[axis3]+radius));
                if (inrangeflag && pointonsphereedge<int,2>(currpoint,center,visibleradius)){
                    //printf("sphere edge\n");
                    currselection.point1[axis1] = 2*center[0]-currpoint[0];
                    currselection.point1[axis2] = 2*center[1]-currpoint[1];
                    currselection.point2[axis1] = currpoint[0];
                    currselection.point2[axis2] = currpoint[1];
                    if (hasdepth){
                        currselection.point1[axis3] = 2*centerpoint[axis3]-pos3;
                        currselection.point2[axis3] = pos3;
                    }
                    currselection.selectionstate=NDL_SELECTIONNEW;
                } else
                if (inrangeflag && pointinsphere<int,2>(currpoint,center,visibleradius)){
                    //printf("sphere grabbed\n");
                    //if we grabbed an existing sphere selection
                    currselection.selectionstate=NDL_SELECTIONMOVE;
                } else {
                    //printf("new sphere\n");
                    
                    //Not an existing selection
                    currselection.point1[axis1] = currpoint[0];
                    currselection.point1[axis2] = currpoint[1];
                    if (hasdepth) currselection.point1[axis3] = pos3;
                    currselection.point2[axis1] = currpoint[0];
                    currselection.point2[axis2] = currpoint[1];
                    if (hasdepth) currselection.point2[axis3] = pos3;
                    currselection.selectiontype=view->tooltype;
                    currselection.selectionstate=NDL_SELECTIONNEW;
                }
                syncviews(view,centerpoint);
                view->activerenderinfopointer->needsrefresh=true;
                break;
            }
        }
    }
    
    static void moveselection(ViewInfo<VoxelType,DIM>* view,SelectionInfo<VoxelType,DIM>& currselection){
        int pos1=(std::max)(0,(std::min)((int)(view->mousex - view->activerenderinfopointer->xpos),(int)(view->activerenderinfopointer->regionsize[view->activerenderinfopointer->axis1]-1)));
        int pos2=(std::max)(0,(std::min)((int)(view->mousey - view->activerenderinfopointer->ypos),(int)(view->activerenderinfopointer->regionsize[view->activerenderinfopointer->axis2]-1)));
        
        int pos3,halfthickness,pos3_lower,pos3_upper;
        if (view->activerenderinfopointer->axis3!=-1){
            pos3 = view->activerenderinfopointer->axis3pos;
            halfthickness = view->activerenderinfopointer->axis3thickness/2;
            pos3_lower = (std::max)(0,pos3-halfthickness);
            pos3_upper = (std::min)(view->activerenderinfopointer->regionsize[view->activerenderinfopointer->axis3]-1,pos3_lower+view->activerenderinfopointer->axis3thickness-1);
            pos3_lower=(std::max)(0,pos3_upper-view->activerenderinfopointer->axis3thickness+1);
        }
        
        if (currselection.selectionstate==NDL_SELECTIONNEW){
            switch(currselection.selectiontype){
                case NDL_POINT_SELECT: {
                    currselection.point1[view->activerenderinfopointer->axis1] = pos1;
                    currselection.point1[view->activerenderinfopointer->axis2] = pos2;
                    currselection.point2[view->activerenderinfopointer->axis1] = pos1;
                    currselection.point2[view->activerenderinfopointer->axis2] = pos2;
                    break;
                }
                case NDL_LINE_SELECT: {
                    currselection.point2[view->activerenderinfopointer->axis1] = pos1;
                    currselection.point2[view->activerenderinfopointer->axis2] = pos2;
                    break;
                }
                case NDL_BOX_SELECT: {
                    currselection.point2[view->activerenderinfopointer->axis1] = pos1;
                    currselection.point2[view->activerenderinfopointer->axis2] = pos2;
                    break;
                }
                case NDL_SPHERE_SELECT: {
                    currselection.point2[view->activerenderinfopointer->axis1] = pos1;
                    currselection.point2[view->activerenderinfopointer->axis2] = pos2;
                    break;
                }
            }
            int centerpoint[DIM];
            for(int i=0;i<DIM;i++) centerpoint[i] = (currselection.point1[i]+currselection.point2[i])/2;
            view->activerenderinfopointer->needsrefresh=true;
            syncviews(view,centerpoint);
        } else
        if (currselection.selectionstate==NDL_SELECTIONMOVE){
            int origpos1=(std::max)(0,(std::min)((int)(view->leftmousedownx - view->activerenderinfopointer->xpos),(int)(view->activerenderinfopointer->regionsize[view->activerenderinfopointer->axis1]-1)));
            int origpos2=(std::max)(0,(std::min)((int)(view->leftmousedowny - view->activerenderinfopointer->ypos),(int)(view->activerenderinfopointer->regionsize[view->activerenderinfopointer->axis2]-1)));
            view->leftmousedownx=view->mousex;
            view->leftmousedowny=view->mousey;
            
            //move both points that define the selection
            currselection.point1[view->activerenderinfopointer->axis1] = (std::max)(0,(std::min)((int)(currselection.point1[view->activerenderinfopointer->axis1] + pos1 - origpos1),(int)(view->activerenderinfopointer->regionsize[view->activerenderinfopointer->axis1]-1)));
            currselection.point1[view->activerenderinfopointer->axis2] = (std::max)(0,(std::min)((int)(currselection.point1[view->activerenderinfopointer->axis2] + pos2 - origpos2),(int)(view->activerenderinfopointer->regionsize[view->activerenderinfopointer->axis2]-1)));
            currselection.point2[view->activerenderinfopointer->axis1] = (std::max)(0,(std::min)((int)(currselection.point2[view->activerenderinfopointer->axis1] + pos1 - origpos1),(int)(view->activerenderinfopointer->regionsize[view->activerenderinfopointer->axis1]-1)));
            currselection.point2[view->activerenderinfopointer->axis2] = (std::max)(0,(std::min)((int)(currselection.point2[view->activerenderinfopointer->axis2] + pos2 - origpos2),(int)(view->activerenderinfopointer->regionsize[view->activerenderinfopointer->axis2]-1)));
            
            //sync views and refresh
            int centerpoint[DIM];
            for(int i=0;i<DIM;i++) centerpoint[i] = (currselection.point1[i]+currselection.point2[i])/2;
            view->activerenderinfopointer->needsrefresh=true;
            syncviews(view,centerpoint);
        } else 
        if (currselection.selectionstate==NDL_SELECTIONMOVEEDGE){
            if (currselection.selectionactivedimension==view->activerenderinfopointer->axis1){
                if (currselection.selectionactiveside==0) currselection.point1[view->activerenderinfopointer->axis1] = pos1;
                else currselection.point2[view->activerenderinfopointer->axis1] = pos1;
            } else 
            if (currselection.selectionactivedimension==view->activerenderinfopointer->axis2){
                if (currselection.selectionactiveside==0) currselection.point1[view->activerenderinfopointer->axis2] = pos2;
                else currselection.point2[view->activerenderinfopointer->axis2] = pos2;
            }
            
            //sync views and refresh
            int centerpoint[DIM];
            for(int i=0;i<DIM;i++) centerpoint[i] = (currselection.point1[i]+currselection.point2[i])/2;
            view->activerenderinfopointer->needsrefresh=true;
            syncviews(view,centerpoint);
        }
    }
    
    static void onmousewheel(ViewInfo<VoxelType,DIM>* view,int delta){
        if (view->activerenderinfopointer->axis3!=-1 && view->activerenderinfopointer){
            int axis3pos = (std::max)(0,(std::min)((int)(view->activerenderinfopointer->axis3pos+delta),(int)(view->activerenderinfopointer->dim[view->activerenderinfopointer->axis3]-1)));
            view->activerenderinfopointer->axis3pos=axis3pos;
            view->activerenderinfopointer->needsrefresh=true;
        }
    }
    
    static void onmousemove(ViewInfo<VoxelType,DIM>* view){
        if (view->leftbutton && view->activerenderinfopointer){
            //check current selection
            moveselection(view,view->selectioninfo);
            
            //check to see if we moved an existing annotation
            if (view->annotationlistpointer){
                for(std::list< SelectionInfo<VoxelType,DIM> >::iterator myiterator = view->annotationlistpointer->begin(); myiterator != view->annotationlistpointer->end(); ++myiterator){
                    moveselection(view,*myiterator);
                }
            }

        }
        if (view->middlebutton && view->activerenderinfopointer){
            if (view->activerenderinfopointer->axis3!=-1){
                int delta = view->middlemousedowny - view->mousey;
                view->middlemousedowny = view->mousey;
                int axis3pos = (std::max)(0,(std::min)((int)(view->activerenderinfopointer->axis3pos+delta),(int)(view->activerenderinfopointer->dim[view->activerenderinfopointer->axis3]-1)));
                view->activerenderinfopointer->axis3pos=axis3pos;
                view->activerenderinfopointer->needsrefresh=true;
            }
        }
        if (view->rightbutton && view->activerenderinfopointer){
            if (view->winlevelcontrols){
                //do window/level stuff...
                double window = view->window;
                double level = view->level;
                level += (view->mousey-view->rightmousedowny);
                window += (view->mousex-view->rightmousedownx);
                view->setwindowlevel(window,level);
            }
        }
    }
    
    //Left: buttonnum=0, Middle: buttonnum=1, Right: buttonnum=2,
    static void onmousebutton(ViewInfo<VoxelType,DIM>* view,int buttonnum){
        
        //left button
        if (buttonnum==0 && view->leftbutton){
            //check to see if we clicked on an existing selection
            clickselection(view,view->selectioninfo);
            
            //check to see if we clicked on an existing annotation
            if (view->annotationlistpointer){
                for(std::list< SelectionInfo<VoxelType,DIM> >::iterator myiterator = view->annotationlistpointer->begin(); myiterator != view->annotationlistpointer->end(); ++myiterator){
                    clickselection(view,*myiterator);
                }
            }
            
        }
        
        //middle button
        if (buttonnum==1 && view->middlebutton){
            //do nothing so far
        }
        
        //right button
        if (buttonnum==2 && view->rightbutton){
            //handle window/level if enabled
            if (view->winlevelcontrols) view->getwindowlevel(&view->window,&view->level);
                
            
        }
    }
    
    
};

}