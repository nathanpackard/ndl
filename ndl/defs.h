#ifndef DEFS_INCLUDED
#define DEFS_INCLUDED

#define NOMINMAX //tell windows to not include min and max macros

//**********************************
//NDL DEFINES
//**********************************

//IO events
#define NDL_NONE 0
#define NDL_WINDOWCLOSED 1
#define NDL_UPDATEDISPLAY 2

//Interpolation Values, equal to the number of required elements for the interpolation
#define NDL_NN 1
#define NDL_LINEAR 2
#define NDL_CUBIC 4

//PROJECTIONTYPES
#define NDL_AVE 0
#define NDL_MIP 1
#define NDL_GEOMAVE 2

//Loop Through All Voxels within Selected Region
 #define NDL_SAVEVARS(_im) \
    const int _DIM = (_im).MYDIM; \
    int *_dim = (_im).m_dimarray; \
    int *_regionsize = (_im).m_regionsize; \
    int *_dimprod = (_im).m_dimPROD; \
    int *_orgin = (_im).m_orgin; \
    int *_dimprodregion = (_im).m_dimPRODregion; \
    int *_numregionvoxelspntr = &(_im).m_numregionvoxels; \
    int _numvoxels = (_im).m_numvoxels; \
    int _numcolors = (_im).m_numcolors;
     
 #define NDL_SAVEVARS2(_im2) \
    const int _DIM2 = (_im2).MYDIM; \
    int *_dim2 = (_im2).m_dimarray; \
    int *_regionsize2 = (_im2).m_regionsize; \
    int *_dimprod2 = (_im2).m_dimPROD; \
    int *_orgin2 = (_im2).m_orgin; \
    int *_dimprodregion2 = (_im2).m_dimPRODregion; \
    int *_numregionvoxelspntr2 = &(_im2).m_numregionvoxels; \
    int _numregionvoxels2 = (_im2).m_numregionvoxels; \
    int _numvoxels2 = (_im2).m_numvoxels; \
    int _numcolors2 = (_im2).m_numcolors;

 #define NDL_SAVESELECTION \
    int _savedorgin[_DIM]; \
    int _savedregion[_DIM]; \
    int _saveddimPRODregion[_DIM]; \
    int _savednumregionvoxels = *_numregionvoxelspntr; \
    for(int _q=0;_q<_DIM;_q++){ \
        _savedorgin[_q]=_orgin[_q]; \
        _savedregion[_q]=_regionsize[_q]; \
        _saveddimPRODregion[_q]=_dimprodregion[_q]; \
    }

 #define NDL_RESTORESELECTION \
    *_numregionvoxelspntr = _savednumregionvoxels; \
    for(int _q=0;_q<_DIM;_q++){ \
        _orgin[_q] = _savedorgin[_q]; \
        _regionsize[_q] = _savedregion[_q]; \
        _dimprodregion[_q] = _saveddimPRODregion[_q]; \
    }

#define NDL_FOREACH(_im) \
    { \
    NDL_SAVEVARS(_im) \
    NDL_SAVESELECTION \
    int _numregionvoxels = (_im).m_numregionvoxels; \
    for(int _c = 0;_c<_numregionvoxels; ++_c)
 
 #define NDL_FOREACHLINE(_im,_axis) \
        { \
        NDL_SAVEVARS(_im) \
        NDL_SAVESELECTION \
        _regionsize[_axis]=1; \
        (_im).select(_orgin,_regionsize); \
        int _numregionvoxels = (_im).m_numregionvoxels; \
        for(int _c = 0;_c<_numregionvoxels; ++_c)

 #define NDL_FOREACHLINE2(_im,_im2,_axis) \
        { \
        NDL_SAVEVARS(_im) \
        NDL_SAVEVARS2(_im2) \
        NDL_SAVESELECTION \
        _regionsize[_axis]=1; \
        (_im).select(_orgin,_regionsize); \
        int _numregionvoxels = (_im).m_numregionvoxels; \
        for(int _c = 0;_c<_numregionvoxels; ++_c)

//Loop Through All Voxels within Selected Region of 1st of 2 images
#define NDL_FOREACH2(_im,_im2) \
     { \
    NDL_SAVEVARS(_im) \
    NDL_SAVEVARS2(_im2) \
    NDL_SAVESELECTION \
    int _numregionvoxels = (_im).m_numregionvoxels; \
    for(int _c = 0;_c<_numregionvoxels; ++_c)

//PARELLEL LOOP IMPLMENTATIONS, IF ENABLED
#if defined(NDL_USE_PPL)
#include <ppl.h>

    //Loop Through All Voxels within Selected Region
    #define NDL_FOREACHPL(_im) \
    { \
        NDL_SAVEVARS(_im) \
        NDL_SAVESELECTION \
        int _numregionvoxels = (_im).m_numregionvoxels; \
        Concurrency::parallel_for(0,_numregionvoxels,[&](int _c)
    
     #define NDL_FOREACHLINEPL(_im,_axis) \
        { \
        NDL_SAVEVARS(_im) \
        NDL_SAVESELECTION \
        _regionsize[_axis]=1; \
        (_im).select(_orgin,_regionsize); \
        int _numregionvoxels = (_im).m_numregionvoxels; \
        Concurrency::parallel_for(0,_numregionvoxels,[&](int _c)

     #define NDL_FOREACHLINE2PL(_im,_im2,_axis) \
        { \
        NDL_SAVEVARS(_im) \
        NDL_SAVEVARS2(_im2) \
        NDL_SAVESELECTION \
        _regionsize[_axis]=1; \
        (_im).select(_orgin,_regionsize); \
        int _numregionvoxels = (_im).m_numregionvoxels; \
        Concurrency::parallel_for(0,_numregionvoxels,[&](int _c)

    //Loop Through All Voxels within Selected Region in 2 images
    #define NDL_FOREACH2PL(_im,_im2) \
     { \
        NDL_SAVEVARS(_im) \
        NDL_SAVEVARS2(_im2) \
        NDL_SAVESELECTION \
        int _numregionvoxels = (_im).m_numregionvoxels; \
        Concurrency::parallel_for(0,_numregionvoxels,[&](int _c)

	#define NDL_ENDFOREACH ); NDL_RESTORESELECTION }
#elif defined(NDL_USE_OMP)

    //Loop Through All Voxels within Selected Region
    #define NDL_FOREACHPL(_im) \
    { \
        NDL_SAVEVARS(_im) \
        NDL_SAVESELECTION \
        int _numregionvoxels = (_im).m_numregionvoxels; \
         __pragma(omp parallel for) \
        for(int _c = 0;_c<_numregionvoxels; ++_c)
    
     #define NDL_FOREACHLINEPL(_im,_axis) \
        { \
        NDL_SAVEVARS(_im) \
        NDL_SAVESELECTION \
        _regionsize[_axis]=1; \
        (_im).select(_orgin,_regionsize); \
        int _numregionvoxels = (_im).m_numregionvoxels; \
         __pragma(omp parallel for) \
        for(int _c = 0;_c<_numregionvoxels; ++_c)

     #define NDL_FOREACHLINE2PL(_im,_im2,_axis) \
        { \
        NDL_SAVEVARS(_im) \
        NDL_SAVEVARS2(_im2) \
        NDL_SAVESELECTION \
        _regionsize[_axis]=1; \
        (_im).select(_orgin,_regionsize); \
        int _numregionvoxels = (_im).m_numregionvoxels; \
         __pragma(omp parallel for) \
        for(int _c = 0;_c<_numregionvoxels; ++_c)

    //Loop Through All Voxels within Selected Region in 2 images
    #define NDL_FOREACH2PL(_im,_im2) \
     { \
        NDL_SAVEVARS(_im) \
        NDL_SAVEVARS2(_im2) \
        NDL_SAVESELECTION \
        int _numregionvoxels = (_im).m_numregionvoxels; \
         __pragma(omp parallel for) \
        for(int _c = 0;_c<_numregionvoxels; ++_c)

    #define NDL_ENDFOREACH NDL_RESTORESELECTION }
#else
    //IF NO PARELELL LIBRARIES ARE USED, JUST USE 
    //NON-PARELELL CODE
    #define NDL_FOREACHPL NDL_FOREACH
    #define NDL_FOREACH2PL NDL_FOREACH2
    #define NDL_FOREACHLINEPL NDL_FOREACHLINE
    #define NDL_FOREACHLINE2PL NDL_FOREACHLINE2
    
    #define NDL_ENDFOREACH NDL_RESTORESELECTION }
#endif

//WITHIN A LOOP, NDL_GET THE 1D-INDEX OF THE CURRENT VOXEL
#define NDL_GETINDEX(_index) \
    _index=_c % _dimprodregion[0]; \
    for(int _n=0;_n<_DIM;++_n){ \
        int _i = (_c / _dimprodregion[_n]) % _regionsize[_n]; \
        int i_plus_orgin = _i+_orgin[_n]; \
        _index += i_plus_orgin*_dimprod[_n]; \
    }
#define NDL_GETCOLORINDEX(_index,_colorindex) \
    _colorindex=_c % _dimprodregion[0]; \
    _index=_colorindex; \
    for(int _n=0;_n<_DIM;++_n){ \
        int _i = (_c / _dimprodregion[_n]) % _regionsize[_n]; \
        int i_plus_orgin = _i+_orgin[_n]; \
        _index += i_plus_orgin*_dimprod[_n]; \
    }

#define NDL_GETINDEX2(_index) \
    _index=_c % _dimprodregion2[0]; \
    for(int _n=0;_n<_DIM2;++_n){ \
        int _i = (_c / _dimprodregion2[_n]) % _regionsize2[_n]; \
        int i_plus_orgin = _i+_orgin2[_n]; \
        _index += i_plus_orgin*_dimprod2[_n]; \
    }

//WITHIN A LOOP, NDL_GET THE 1D-INDEX AND ND-COORDENATE OF THE CURRENT VOXEL
#define NDL_GETCOORD(_index,_coord) \
    _index=_c % _dimprodregion[0]; \
    for(int _n=0;_n<_DIM;++_n){ \
        int _i = (_c / _dimprodregion[_n]) % _regionsize[_n]; \
        int i_plus_orgin = _i+_orgin[_n]; \
        _coord[_n] = i_plus_orgin; \
        _index += i_plus_orgin*_dimprod[_n]; \
    }
#define NDL_GETCOLORCOORD(_index,_colorindex,_coord) \
    _colorindex=_c % _dimprodregion[0]; \
    _index=_colorindex; \
    for(int _n=0;_n<_DIM;++_n){ \
        int _i = (_c / _dimprodregion[_n]) % _regionsize[_n]; \
        int i_plus_orgin = _i+_orgin[_n]; \
        _coord[_n] = i_plus_orgin; \
        _index += i_plus_orgin*_dimprod[_n]; \
    }    

#define NDL_GETCOORD2(_index,_coord) \
    _index=_c % _dimprodregion2[0]; \
    for(int _n=0;_n<_DIM2;++_n){ \
        int _i = (_c / _dimprodregion2[_n]) % _regionsize2[_n]; \
        int i_plus_orgin2 = _i+_orgin2[_n]; \
        _coord[_n] = i_plus_orgin2; \
        _index += i_plus_orgin*_dimprod2[_n]; \
    }
    
#define NDL_GETCOORD_FOR_TRANSFORM(_index,_coord) \
    _index=_c % _dimprodregion[0]; \
    for(int _n=0;_n<_DIM2;++_n){ \
        int _i = (_c / _dimprodregion[_n]) % _regionsize[_n]; \
        int i_plus_orgin = _i+_orgin2[_n]; \
        _coord[_n] = i_plus_orgin; \
        _index += i_plus_orgin*_dimprod2[_n]; \
    }

//WITHIN A LOOP, AND GIVEN THE INDEX AND COORD (FROM NDL_GETCOORD) AND DATA ARRAY
//RETURN VALUES OF CROSS SHAPED FILTER SURROUNDING THE CURRENT VOXEL
#define NDL_NUMCROSSVALUES ((_DIM * 2)+1)
#define NDL_GETCROSSVALUES(_index,_coord,_thedimprod,_data,_values) \
    { \
        _values[0] = _data[_index]; \
        for(int _n=0;_n<_DIM;++_n){ \
            int _t=_coord[_n]; \
            if (_t >= 1){ _values[(_n<<1) + 1] = _data[_index - _thedimprod[_n]]; } \
            else _values[(_n<<1) + 1] = _values[0]; \
            if (_t < m_dimarray[_n]-1){ _values[(_n<<1) + 2] = _data[_index + _thedimprod[_n]]; } \
            else _values[(_n<<1) + 2] = _values[0]; \
        } \
    }
    
//WITHIN A LOOP, GET THE 1D-INDEX OF THE CURRENT VOXEL FOR TWO IMAGES
 #define NDL_GETINDEXPAIR(_validflag,_index,_index2) \
    _index=_c % _dimprodregion[0]; \
    _index2=_c % _dimprodregion2[0]; \
    _validflag=1;\
    for(int _n=0;_n<_DIM;++_n){ \
        int _i = (_c / _dimprodregion[_n]) % _regionsize[_n]; \
        int i_plus_orgin = _i+_orgin[_n]; \
        int i_plus_orgin2 = _i+_orgin2[_n]; \
        if (i_plus_orgin2 < _dim2[_n] && i_plus_orgin2>=0){ \
            _index += i_plus_orgin*_dimprod[_n]; \
            _index2 += i_plus_orgin2*_dimprod2[_n]; \
        } else { \
            _validflag=0;\
            break; \
        } \
    }


//WITHIN A LOOP, GET THE 1D-INDEX AND ND-COORDENATE OF THE CURRENT VOXEL FOR TWO IMAGES
#define NDL_GETCOORDPAIR(_validflag,_index,_coord,_index2,_coord2) \
    _index=_c % _dimprodregion[0]; \
    _index2=_c % _dimprodregion2[0]; \
    _validflag=1;\
    for(int _n=0;_n<_DIM;++_n){ \
        int _i = (_c / _dimprodregion[_n]) % _regionsize[_n]; \
        int i_plus_orgin = _i+_orgin[_n]; \
        int i_plus_orgin2 = _i+_orgin2[_n]; \
        if (i_plus_orgin2 < _dim2[_n] && i_plus_orgin2>=0){ \
            _coord[_n] = i_plus_orgin; \
            _coord2[_n] = i_plus_orgin2; \
            _index += i_plus_orgin*_dimprod[_n]; \
            _index2 += i_plus_orgin2*_dimprod2[_n]; \
        } else { _validflag=0; break; } \
    }

#endif