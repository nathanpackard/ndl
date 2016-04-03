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

namespace ndl {
    
#include <jpeglib.h> //avoid namespace conflicts by putting this here
#include <setjmp.h>

/*!
JPEG Image Format Plugin.
To use this plugin, you must type
#define NDL_USE_JPEG
before you include ndl.h in your
project. Also, you must
have jpeglib.h in your include path.
The jpeglib library is also 
requried when using this plugin, be sure
to link your project to the following .lib file:
- libjpeg.lib
*/

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;
    
//Here's the routine that will replace the standard error_exit method:
METHODDEF(void)
my_error_exit (j_common_ptr cinfo){
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

//Here's the routine that will replace the standard output_message method:
METHODDEF(void) my_output_message (j_common_ptr cinfo){ }
    
template<class VoxelType,int DIM>
class JpegPlugin {
public:
    static std::string imagetypename(){ return "jpeg"; }
    static std::string imageopenextension(){ return "jpg"; }
    static std::string imagesaveextension(){ return "jpg"; }
    static bool isfilecompatible(std::string filename,std::string flags,bool sequenceflag=false,bool exactflag=false){
        if (exactflag && getdatatype<VoxelType>()!=G_UNSIGNED_CHAR) return false;
        if (DIM<2 || (DIM<3 && sequenceflag)) return false;
        
        struct jpeg_decompress_struct cinfo;
        struct my_error_mgr jerr;
        FILE* infile = fopen(filename.c_str(),"rb");
        if (infile){
            cinfo.err = jpeg_std_error( &jerr.pub );
            jerr.pub.error_exit = my_error_exit;
            jerr.pub.output_message = my_output_message;
            jpeg_create_decompress( &cinfo ); // setup decompression process and source, then read JPEG header
            jpeg_stdio_src( &cinfo, infile ); // this makes the library read from infile
            
            //Establish the setjmp return context for my_error_exit to use.
            if (setjmp(jerr.setjmp_buffer)){
                //If we get here, the JPEG code has signaled an error.
                //We need to clean up the JPEG object, close the input file, and return.
                jpeg_destroy_decompress(&cinfo);
                fclose(infile);
                return false;
            }
            
            if (jpeg_read_header( &cinfo, TRUE ) == JPEG_HEADER_OK){
                fclose(infile);
                return true;
            } else {
                fclose(infile);
                return false;
            }
        } else return false;
    }
        
    static bool loadfile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        //check if valid dimensions and type
        int numinsequence = sequence.size();
        int difftype = (getdatatype<VoxelType>()!=G_UNSIGNED_CHAR);
        if (exactmatchonly && (difftype)) return false;
        if (DIM<2 || (DIM<3 && numinsequence>1)) return false;
        
        //read the file information and setup the image
        struct my_error_mgr jerr;
        struct jpeg_decompress_struct* cinfo;
        struct jpeg_decompress_struct defaultheader;
        if (headerpntr) cinfo = static_cast<jpeg_decompress_struct*>(headerpntr);
        else cinfo = &defaultheader;
        FILE* infile = fopen(sequence[0].c_str(),"rb");
        if (infile){
            cinfo->err = jpeg_std_error( &jerr.pub ); // here we set up the standard libjpeg error handler
            jpeg_create_decompress( cinfo ); // setup decompression process and source, then read JPEG header
            jpeg_stdio_src( cinfo, infile ); // this makes the library read from infile
            jpeg_read_header( cinfo, TRUE ); // reading the image header which contains image information
            
            // printf( "JPEG File Information: \n" );
            // printf( "Image width and height: %d pixels and %d pixels.\n", cinfo->image_width, cinfo.image_height );
            // printf( "Color components per pixel: %d.\n", cinfo.num_components );
            // printf( "Color space: %d.\n", cinfo.jpeg_color_space );
        }
        im.m_dimarray[0]=cinfo->image_width;
        im.m_dimarray[1]=cinfo->image_height;
        if (DIM>2) im.m_dimarray[2]=numinsequence;
        im.setupdimensions(im.m_dimarray,cinfo->num_components);
        im.setupdata();
        
        //load the image data into the image
        JSAMPROW row_pointer[1]; //libjpeg data structure for storing one row, that is, scanline of an image
        int imagesize = im.m_dimarray[0]*im.m_dimarray[1];
        unsigned char* datapntr = new unsigned char[imagesize*cinfo->num_components];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++) {
            sprintf(text,"loading %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            int offset = i*imagesize*cinfo->num_components;
            
            FILE* infile = fopen(sequence[i].c_str(),"rb");
            if (infile){
                cinfo->err = jpeg_std_error( &jerr.pub ); // here we set up the standard libjpeg error handler
                jpeg_create_decompress( cinfo ); // setup decompression process and source, then read JPEG header
                jpeg_stdio_src( cinfo, infile ); // this makes the library read from infile
                jpeg_read_header( cinfo, TRUE ); // reading the image header which contains image information
                
                //Start decompression jpeg here
                jpeg_start_decompress( cinfo );

                // now actually read the jpeg into the raw buffer
                row_pointer[0] = (unsigned char *)malloc( cinfo->output_width*cinfo->num_components );
                
                // read one scan line at a time
                unsigned long location = 0;
                while( cinfo->output_scanline < cinfo->image_height ){
                    jpeg_read_scanlines( cinfo, row_pointer, 1 );
                    for(int i=0; i<cinfo->image_width*cinfo->num_components;i++)
                        datapntr[location++] = row_pointer[0][i];
                }
                for (int t=0;t<imagesize*cinfo->num_components;t++) im.m_data[offset+t]=datapntr[t];
                
                // wrap up decompression, destroy objects, free pointers and close open files
                jpeg_finish_decompress( cinfo );
                jpeg_destroy_decompress( cinfo );
                free( row_pointer[0] );
                fclose( infile );
            } else {
                std::cout << "Error opening file: " << sequence[i] << std::endl;
                return false;
            }
        }
        im.updateprogress(text,100);
        
        //remember the filetype
        im.m_filetype = imagetypename();
        return true;
    }
    
    static bool savefile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        //check if valid dimensions and type
        int numinsequence = sequence.size();
        if (DIM<2 || (DIM<3 && numinsequence>1)) return false;
        int difftype = (getdatatype<VoxelType>()!=G_UNSIGNED_CHAR);
        if (exactmatchonly && difftype) return false;
        
        //set the file information from the image
        struct jpeg_compress_struct* cinfo;
        struct jpeg_compress_struct defaultheader;
        if (headerpntr) cinfo = static_cast<jpeg_compress_struct*>(headerpntr);
        else cinfo = &defaultheader;
        struct jpeg_error_mgr jerr;
        JSAMPROW row_pointer[1];
                
        //save the image data
        int imagesize = im.m_dimarray[0]*im.m_dimarray[1];
        unsigned char* datapntr;
        if (im.m_numcolors==1) datapntr = new unsigned char[imagesize];
        else datapntr = new unsigned char[imagesize*3];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++) {
            sprintf(text,"saving %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            
            //update header stuff for this image
            //... nothing to do
            
            //save
            int offset = i*imagesize*im.m_numcolors;
            
            //scale/bias the data to fit within the range of 0..255
            double minvalue=im.m_min;
            double maxvalue=im.m_max;
            if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR){ minvalue = 0; maxvalue=255; }
            if (getdatatype<VoxelType>()==G_CHAR){ minvalue = -128; maxvalue=127;}
            double bias = -minvalue;
            double scale = 255.0/(maxvalue-minvalue);
            //printf("bias: %f, scale: %f\n",bias,scale);
            //printf("minvalue: %f, maxvalue: %f\n",minvalue,maxvalue);
            
            //save the jpeg file
            cinfo->err = jpeg_std_error( &jerr );
            FILE *outfile = fopen( sequence[i].c_str(), "wb" );
            if ( outfile ){
                int c=0;
                for (int t=0;t<imagesize;t++){
                    if (im.m_numcolors==1) datapntr[t] = scale*(im.m_data[offset+t] + bias) + 0.5;
                    else { //i.e.: im.m_numcolors is >1
                        float value = scale*(im.m_data[offset+c] + bias) + 0.5;
                        datapntr[t*3] = CLAMP(value,minvalue,maxvalue); 
                        c++;
                        value = scale*(im.m_data[offset+c] + bias) + 0.5;
                        datapntr[t*3+1] = CLAMP(value,minvalue,maxvalue); 
                        c++;
                        if (im.m_numcolors>2){
                            value = scale*(im.m_data[offset+c] + bias) + 0.5;
                            datapntr[t*3+2] = CLAMP(value,minvalue,maxvalue); 
                            c+= im.m_numcolors - 2;
                        }
                    }
                }
                
                // printf( "Saving JPEG File, Information: \n" );
                // printf( "Image width and height: %d pixels and %d pixels.\n", cinfo->image_width, cinfo->image_height );
                // printf( "Color components per pixel: %d.\n", cinfo->num_components );
                // printf( "Color space: %d.\n", cinfo->jpeg_color_space );
                
                // Now do the compression ..
                jpeg_create_compress(cinfo);
                jpeg_stdio_dest(cinfo, outfile);
                cinfo->image_width = im.m_dimarray[0];
                cinfo->image_height = im.m_dimarray[1];
                if (im.m_numcolors==1){
                    cinfo->in_color_space = (J_COLOR_SPACE)JCS_GRAYSCALE;
                    cinfo->input_components = cinfo->num_components = 1;
                    jpeg_set_defaults(cinfo);
                    //jpeg_set_quality(&cinfo,100,true);
                    jpeg_start_compress( cinfo, TRUE );
                    // like reading a file, this time write one row at a time
                    while( cinfo->next_scanline < cinfo->image_height ){
                        row_pointer[0] = &datapntr[ cinfo->next_scanline * cinfo->image_width *  cinfo->input_components];
                        jpeg_write_scanlines( cinfo, row_pointer, 1 );
                    }
                } else {
                    cinfo->in_color_space = (J_COLOR_SPACE)JCS_RGB;
                    cinfo->input_components = cinfo->num_components = 3;
                    jpeg_set_defaults(cinfo);
                    //jpeg_set_quality(&cinfo,100,true);
                    jpeg_start_compress( cinfo, TRUE );
                    // like reading a file, this time write one row at a time
                    while( cinfo->next_scanline < cinfo->image_height ){
                        row_pointer[0] = &datapntr[ cinfo->next_scanline * cinfo->image_width *  cinfo->input_components];
                        jpeg_write_scanlines( cinfo, row_pointer, 1 );
                    }
                }
                
                // similar to read file, clean up after we're done compressing
                jpeg_finish_compress( cinfo );
                jpeg_destroy_compress( cinfo );
                fclose( outfile );
            } else {
                printf("Error opening output jpeg file %s\n!", sequence[i].c_str() );
                return false;
            }
        }
        im.updateprogress(text,100);
            
        //save the filetype
        im.m_filetype = imagetypename();
        return true;
    }
};

}