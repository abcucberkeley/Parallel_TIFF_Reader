//#include "tiffio.h"
#include <stdio.h>
#include <stdint.h>
#include "tiffio.h"
#include "omp.h"
#include "mex.h"
//mex -v COPTIMFLAGS="-O3 -fwrapv -DNDEBUG" CFLAGS='$CFLAGS -O3 -fopenmp' LDFLAGS='$LDFLAGS -O3 -fopenmp' '-I/global/home/groups/software/sl-7.x86_64/modules/libtiff/4.1.0/libtiff/' '-L/global/home/groups/software/sl-7.x86_64/modules/libtiff/4.1.0/libtiff/' -ltiff /clusterfs/fiona/matthewmueller/parallelTiffTesting/main.c
//mex COMPFLAGS='$COMPFLAGS /openmp' '-IC:\Program Files (x86)\tiff\include\' '-LC:\Program Files (x86)\tiff\lib\' -ltiffd.lib C:\Users\Matt\Documents\parallelTiff\main.cpp


void DummyHandler(const char* module, const char* fmt, va_list ap)
{
    // ignore errors and warnings
}

void* mallocDynamic(uint64_t x, uint64_t bits){
    switch(bits){
        case 8:
            return malloc(x*sizeof(uint8_t));
        case 16:
            return malloc(x*sizeof(uint16_t));
        case 32:
            return malloc(x*sizeof(float));
        case 64:
            return malloc(x*sizeof(double));
        default:
            printf("Image is not 8/16 bit, single, or double. Using single.");
            return malloc(x*sizeof(float));
    }
}
        
void readTiffParallel(uint64_t x, uint64_t y, uint64_t z, char* fileName, void* tiff, uint64_t bits, uint64_t startSlice, uint64_t stripSize){
    int32_t numWorkers = omp_get_max_threads();
    int32_t batchSize = (z-1)/numWorkers+1;
    
    int32_t w;
    uint8_t err = 0;
    char errString[10000];
    #pragma omp parallel for
    for(w = 0; w < numWorkers; w++){
        
        TIFF* tif = TIFFOpen(fileName, "r");
        if(!tif){
            #pragma omp critical
            {
            err = 1;
            sprintf(errString,"Thread %d: File \"%s\" cannot be opened\n",w,fileName);
            }
        }
        void* buffer = mallocDynamic(x*stripSize, bits);
        for(int64_t dir = startSlice+(w*batchSize); dir < startSlice+((w+1)*batchSize); dir++){
            if(dir>=z+startSlice || err) break;
            
            uint8_t counter = 0; 
            while(!TIFFSetDirectory(tif, (uint64_t)dir) && counter<3){
                printf("Thread %d: File \"%s\" Directory \"%d\" failed to open. Try %d\n",w,fileName,dir,counter+1);
                counter++;
            }

            for (int64_t i = 0; i*stripSize < y; i++) 
            {
                
                //loading the data into a buffer
                switch(bits){
                    case 8:
                        // Map Values to flip x and y for MATLAB
                        TIFFReadEncodedStrip(tif, i,(uint8_t*)buffer, stripSize*x*(bits/8));
                        for(int64_t k = 0; k < stripSize; k++){
                            if((k+(i*stripSize)) >= y) break;
                            for(int64_t j = 0; j < x; j++){
                                ((uint8_t*)tiff)[((j*y)+(k+(i*stripSize)))+((dir-startSlice)*(x*y))] = ((uint8_t*)buffer)[j+(k*x)];
                            }
                        }
                        break;
                    case 16:
                        // Map Values to flip x and y for MATLAB
                        TIFFReadEncodedStrip(tif, i,(uint16_t*)buffer, stripSize*x*(bits/8));
                        for(int64_t k = 0; k < stripSize; k++){
                            if((k+(i*stripSize)) >= y) break;
                            for(int64_t j = 0; j < x; j++){
                                ((uint16_t*)tiff)[((j*y)+(k+(i*stripSize)))+((dir-startSlice)*(x*y))] = ((uint16_t*)buffer)[j+(k*x)];
                            }
                        }
                        break;
                    case 32:
                        // Map Values to flip x and y for MATLAB
                        TIFFReadEncodedStrip(tif, i,(float*)buffer, stripSize*x*(bits/8));
                        for(int64_t k = 0; k < stripSize; k++){
                            if((k+(i*stripSize)) >= y) break;
                            for(int64_t j = 0; j < x; j++){
                                ((float*)tiff)[((j*y)+(k+(i*stripSize)))+((dir-startSlice)*(x*y))] = ((float*)buffer)[j+(k*x)];
                            }
                        }
                        break;
                    case 64:
                        // Map Values to flip x and y for MATLAB
                        TIFFReadEncodedStrip(tif, i,(double*)buffer, stripSize*x*(bits/8));
                        for(int64_t k = 0; k < stripSize; k++){
                            if((k+(i*stripSize)) >= y) break;
                            for(int64_t j = 0; j < x; j++){
                                ((double*)tiff)[((j*y)+(k+(i*stripSize)))+((dir-startSlice)*(x*y))] = ((double*)buffer)[j+(k*x)];
                            }
                        }
                        break;
                }
            }
        }
        free(buffer);
        TIFFClose(tif);
    }
    if(err) mexErrMsgIdAndTxt("tiff:threadError",errString);
}

void mexFunction(int nlhs, mxArray *plhs[],
        int nrhs, const mxArray *prhs[])
{
    char* fileName = mxArrayToString(prhs[0]);
    
    TIFFSetWarningHandler(DummyHandler);
    TIFF* tif = TIFFOpen(fileName, "r");
    if(!tif) mexErrMsgIdAndTxt("tiff:inputError","File \"%s\" cannot be opened",fileName);
    
    uint64_t x = 1,y = 1,z = 1,bits = 1, startSlice = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &x);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &y);
    
    if(nrhs == 1){
        uint16_t s = 0, m = 0, t = 1;
        while(TIFFSetDirectory(tif,t)){
            s = t;
            t *= 8;
            if(s > t){ 
                t = 65535;
                printf("Number of slices > 32768");
                break;
            }
        }
        while(s != t){
            m = (s+t+1)/2;
            if(TIFFSetDirectory(tif,m)){
                s = m;
            }
            else{
                if(m > 0) t = m-1;
                else t = m;
            }
        }
        z = s+1;
    }
    else{
        if(mxGetN(prhs[1]) != 2){
            mexErrMsgIdAndTxt("tiff:inputError","Input range is not 2");
        }
        else{   
            startSlice = (uint64_t)*(mxGetPr(prhs[1]))-1;
            z = (uint64_t)*((mxGetPr(prhs[1])+1))-startSlice;
            if (!TIFFSetDirectory(tif,startSlice+z-1) || !TIFFSetDirectory(tif,startSlice)){
                mexErrMsgIdAndTxt("tiff:rangeOutOfBound","Range is out of bounds");
            }
        }        
    }
  
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits);
    uint64_t stripSize = 1;
    TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &stripSize);
    TIFFClose(tif);
    uint64_t dim[3];
    dim[0] = y;
    dim[1] = x;
    dim[2] = z;

    if(bits == 8){
        plhs[0] = mxCreateNumericArray(3,dim,mxUINT8_CLASS, mxREAL);
        uint8_t* tiff = (uint8_t*)mxGetPr(plhs[0]);
        readTiffParallel(x,y,z,fileName, (void*)tiff, bits, startSlice, stripSize);
    }
    else if(bits == 16){
        plhs[0] = mxCreateNumericArray(3,dim,mxUINT16_CLASS, mxREAL);
        uint16_t* tiff = (uint16_t*)mxGetPr(plhs[0]);
        readTiffParallel(x,y,z,fileName, (void*)tiff, bits, startSlice, stripSize);
    }
    else if(bits == 32){
        plhs[0] = mxCreateNumericArray(3,dim,mxSINGLE_CLASS, mxREAL);
        float* tiff = (float*)mxGetPr(plhs[0]);
        readTiffParallel(x,y,z,fileName, (void*)tiff, bits, startSlice, stripSize);
    }
    else if(bits == 64){
        plhs[0] = mxCreateNumericArray(3,dim,mxDOUBLE_CLASS, mxREAL);
        double* tiff = (double*)mxGetPr(plhs[0]);
        readTiffParallel(x,y,z,fileName, (void*)tiff, bits, startSlice, stripSize);
    }
    else{
        mexErrMsgIdAndTxt("tiff:dataTypeError","Data type not suppported");
    }
}
