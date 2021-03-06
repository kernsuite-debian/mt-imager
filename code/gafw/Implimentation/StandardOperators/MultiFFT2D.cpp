/* MultiFFT2D.cpp:  C++ implementation of the MultiFFT2D operator 
 *      
 * Copyright (C) 2013  Daniel Muscat
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author's contact details can be found at http://www.danielmuscat.com 
 *
 */
#include <cuda_runtime.h>
#include "cufft.h"
#include "GPUafw.h"
#include <cuComplex.h>
#include "cuFFTException.h"
#include "MultiFFT2D.h"

using namespace GAFW::GPU::StandardOperators;
using namespace GAFW::GPU;
using namespace GAFW::GeneralImplimentation;

MultiFFT2D::MultiFFT2D(GAFW::GPU::GPUFactory * factory,std::string nickname):GPUArrayOperator(factory,nickname,string("Operator::MultiFFT2D"))
{


}
MultiFFT2D::~MultiFFT2D() {
}
void MultiFFT2D::validate(GAFW::GPU::ValidationData &valData)
{
    valData.bufferMode=GAFW::GPU::Buffer::Buffer_UnkownSize;
    valData.bufferSize=0;
    this->validate();
}
void MultiFFT2D::validate()
{
    
  
   //No of inputs
    if (this->inputs.size()!=1) throw ValidationException("Only 1 input is supported");
    if (this->outputs.size()!=1) throw ValidationException("Only 1 output is supported");
    //Ok they seem to be equal but have to make sure that infact they have been set
    if (!this->inputs[0]->isDefined()) throw ValidationException("Input is not defined");
    
    switch (this->inputs[0]->getType())
    {
        case complex_float:
        case complex_double:
            //OK
            break;
        case real_float:
            throw ValidationException("At current MultiFFT2D of a real array is not supported");
        default:
            throw ValidationException("Unsupported store type of input");
            
    }
    
    //We expect input to be a 2D Matrix
    
    ArrayDimensions d=this->inputs[0]->getDimensions();
    if (d.getNoOfDimensions()<2) throw ValidationException("MultiFFT2D can work on arrays of 2 dimensions or above!")
    this->outputs[0]->setDimensions(d);
    this->outputs[0]->setType(this->inputs[0]->getType());
}


void MultiFFT2D::submitToGPU(GAFW::GPU::GPUSubmissionData &data)
{
    cufftHandle *p=new cufftHandle();
    *data.postExecutePointer=p;
    cufftHandle &plan=*p; 
    cufftResult result;
     //We expect one input and one output
    switch (data.inputs->type)
     {
         case complex_float:
         { 
             result=cufftPlan2d(&plan,data.inputs->dim.getNoOfRows(),data.inputs->dim.getNoOfColumns(),CUFFT_C2C);
             if (result!=CUFFT_SUCCESS) throw FFTException("Execution of cufftPlan2d() failed",result);
             
             result=cufftSetCompatibilityMode(plan,CUFFT_COMPATIBILITY_FFTW_ALL);
             if (result!=CUFFT_SUCCESS) 
                 throw FFTException("Execution of cufftSetCompatibilityMode() failed",result);
             // due to an issue on cuFFT execution of FFT has to be done on stream 0.
             result=cufftSetStream(plan,/*data.stream*/0);
             if (result!=CUFFT_SUCCESS)
                 throw FFTException("Execution of cufftSetdata.stream() failed",result);
             
             int sizeOfXYPlane=data.inputs->dim.getNoOfRows()*data.inputs->dim.getNoOfColumns();
             int noOfFFTs=data.inputs->dim.getTotalNoOfElements()/sizeOfXYPlane;
             //WE must make sure that nothing is running on data.stream
             checkCudaError(cudaStreamSynchronize(data.stream),"Unable to synchronise");
             //Re-record start event on stream 0
             checkCudaError(cudaEventRecord(*(data.startEvent),0),"Unable to record event");
             for (int i=0;i<noOfFFTs;i++)
             {
                result=cufftExecC2C(plan,((cuComplex *)data.inputs->pointer)+i*sizeOfXYPlane,((cuComplex *)data.outputs->pointer)+i*sizeOfXYPlane,CUFFT_FORWARD);
                if (result!=CUFFT_SUCCESS)
                {   throw FFTException("Execution of cufftExecC2C() failed",result);

                }
             }
             //Record end event on stream 0
             checkCudaError(cudaEventRecord(*(data.endEvent),0),"Unable to record event");
             //and wait until all FFTs are finished on stream 0..
             checkCudaError(cudaStreamSynchronize(0),"Unable to synchronise");
         }   
             break;
        case complex_double:
        { 
             result=cufftPlan2d(&plan,data.inputs->dim.getNoOfRows(),data.inputs->dim.getNoOfColumns(),CUFFT_Z2Z);
             if (result!=CUFFT_SUCCESS) throw FFTException("Execution of cufftPlan2d() failed",result);
             
             result=cufftSetCompatibilityMode(plan,CUFFT_COMPATIBILITY_NATIVE);
             if (result!=CUFFT_SUCCESS) 
                 throw FFTException("Execution of cufftSetCompatibilityMode() failed",result);
             // due to an issue on cuFFT execution of FFT has to be done on stream 0.
             result=cufftSetStream(plan,/*data.stream*/0);
             if (result!=CUFFT_SUCCESS)
                 throw FFTException("Execution of cufftSetdata.stream() failed",result);
             
             int sizeOfXYPlane=data.inputs->dim.getNoOfRows()*data.inputs->dim.getNoOfColumns();
             int noOfFFTs=data.inputs->dim.getTotalNoOfElements()/sizeOfXYPlane;

             //WE must make sure that nothing is running on data.stream
             checkCudaError(cudaStreamSynchronize(data.stream),"Unable to synchronise");
             //Re-record start event on stream 0
             checkCudaError(cudaEventRecord(*(data.startEvent),0),"Unable to record event");
            
             for (int i=0;i<noOfFFTs;i++)
             {
                result=cufftExecZ2Z(plan,((cuDoubleComplex *)data.inputs->pointer)+i*sizeOfXYPlane,((cuDoubleComplex *)data.outputs->pointer)+i*sizeOfXYPlane,CUFFT_FORWARD);
                if (result!=CUFFT_SUCCESS)
                {   throw FFTException("Execution of cufftExecZ2Z() failed",result);

                }
             }
            //Record end event on stream 0
             checkCudaError(cudaEventRecord(*(data.endEvent),0),"Unable to record event");
             //and wait until all FFTs are finished on stream 0..
             checkCudaError(cudaStreamSynchronize(0),"Unable to synchronise");

        }
             break;
            
        
        default:
             throw Bug("BUG:Unknown store type for FFT2D. Validation should had already stopped the execution");
     }
    
     
}
void MultiFFT2D::postRunExecute(void *data)
{
    logDebug(execution,"Post execution:Deleting plan");
    cufftDestroy(*((cufftHandle* )data));
}