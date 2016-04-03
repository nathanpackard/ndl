namespace ndl {

template<class VoxelType,int DIM>
class Render1D {
public:
    static ViewInfo<VoxelType,DIM>* openview(Image<VoxelType,DIM>& image,ViewInfo<VoxelType,DIM>* view){
        return 0;
    }
    static void renderimage(ViewInfo<VoxelType,DIM>* view,RenderInfo<VoxelType,DIM>& renderinfo,Image<unsigned char,2>& displayimage){
    }
    static void renderannotation(ViewInfo<VoxelType,DIM>* view,RenderInfo<VoxelType,DIM>& renderinfo,Image<unsigned char,2>& displayimage){
    }
    static void renderselection(ViewInfo<VoxelType,DIM>* view,RenderInfo<VoxelType,DIM>& renderinfo,Image<unsigned char,2>& displayimage){
    }
    static void onmousewheel(ViewInfo<VoxelType,DIM>* view,int delta){
    }
    static void onmousemove(ViewInfo<VoxelType,DIM>* view){
    }
    static void onmousebutton(ViewInfo<VoxelType,DIM>* view,int buttonnum){
    }
};

}