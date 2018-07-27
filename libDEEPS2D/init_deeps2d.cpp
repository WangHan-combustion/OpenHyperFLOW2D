/*******************************************************************************
*   OpenHyperFLOW2D-CUDA                                                       *
*                                                                              *
*   Transient, Density based Effective Explicit Parallel Hybrid Solver         *
*   TDEEPHS (CUDA+MPI)                                                         *
*   Version  2.0.1                                                             *
*   Copyright (C)  1995-2016 by Serge A. Suchkov                               *
*   Copyright policy: LGPL V3                                                  *
*   http://github.com/sergeas67/openhyperflow2d                                *
*                                                                              *
*   init_deeps2d.cpp: OpenHyperFLOW2D solver init code....                     *
*                                                                              *
*  last update: 14/04/2016                                                     *
********************************************************************************/
#include "deeps2d_core.hpp"


#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/file.h>
int NumContour;
int isCalibrate;
int start_iter  = 5;
int num_threads = 1;
int num_blocks  = 1;

SourceList2D*  SrcList = NULL;
int            isGasSource=0;
int            isRecalcYplus;
int            isLocalTimeStep;
int            isHighOrder;
int            Chemical_reactions_model;
#ifdef   _RMS_
int            isAlternateRMS;
#endif //_RMS_ 
int            isIgnoreUnsetNodes;
int            TurbStartIter;
int            TurbExtModel;
int            err_i, err_j;
int            turb_mod_name_index = 0;
FP             Ts0,A,W,Mach,nrbc_beta0;

unsigned int*  dt_min_host;
unsigned int*  dt_min_device;

UArray< unsigned int* >* dt_min_host_Array;
UArray< unsigned int* >* dt_min_device_Array;

UArray< XY<int> >* GlobalSubDomain;
UArray< XY<int> >* WallNodes;

UArray<UMatrix2D< FlowNode2D<FP,NUM_COMPONENTS> >*>*     SubDomainArray;
UArray<UMatrix2D< FlowNodeCore2D<FP,NUM_COMPONENTS> >*>* CoreSubDomainArray;
UArray<XCut>*                                            XCutArray;
UArray<OutVariable>*                                     OutVariables_Array;

FP  makeZero;

int     Nstep;
FP      ExitMonitorValue;
int     MonitorIndex;
int     MonitorCondition; // 0 - equal
                          // 1 - less than
                          // 2 - great than

unsigned int     AddSrcStartIter = 0;
FP  beta;
FP  beta0;
FP  CFL0;
Table*  CFL_Scenario;
Table*  beta_Scenario;
#ifdef _OPEN_MP
FP  DD_max_var;
#endif // OPEN_MP

//------------------------------------------
// Cx,Cy,Cd,Cv,Cp,St
//------------------------------------------
int     is_Cx_calc,is_Cd_calc;
FP      p_ambient;
FP      x0_body,y0_body,dx_body,dy_body;
FP      x0_nozzle,y0_nozzle,dy_nozzle;

int     Cx_Flow_index;
int     Cd_Flow_index;
int     Cp_Flow_index;
int     SigmaFi_Flow_index;
int     y_max,y_min;
//------------------------------------------
int     NOutStep;
int     isFirstStart=0;
int     ScaleFLAG=1;
int     isAdiabaticWall;
int     isOutHeatFluxX;
int     isOutHeatFluxY;

ChemicalReactionsModelData2D chemical_reactions;
BlendingFactorFunction       bFF;

FP* Y=NULL;
FP  Cp=0.;
FP  lam=0.;
FP  mu=0.;
FP  Tg=0.;
FP  Rg=0.;
FP  Pg=0.;
FP  Wg=0.;
FP  Ug,Vg;
int     CompIndex;

FP Y_fuel[4]={1.,0.,0.,0.};  /* fuel */
FP Y_ox[4]  ={0.,1.,0.,0.};  /* OX */
FP Y_cp[4]  ={0.,0.,1.,0.};  /* cp */
FP Y_air[4] ={0.,0.,0.,1.};  /* air */
FP Y_mix[4] ={0.,0.,0.,0.};  /* mixture */
#ifdef _RMS_
const  char*                                 RMS_Name[11] = {"Rho","Rho*U","Rho*V","Rho*E","Rho*Yfu","Rho*Yox","Rho*Ycp","Rho*k","Rho*eps","Rho*omega","nu_t"};
char                                         RMSFileName[255];
ofstream*                                    pRMS_OutFile;      // Output RMS stream
#endif //_RMS_

char                                         MonitorsFileName[255];
ofstream*                                    pMonitors_OutFile; // Output Monitors stream

int                                          useSwapFile=0;
char                                         OldSwapFileName[255];
void*                                        OldSwapData;
u_long                                       OldFileSizeGas;
int                                          Old_fd;
int                                          isScan;
int                                          isPiston;

int                                          MemLen;
int                                          isRun=0;
int                                          isDraw=0;
int                                          isSnapshot=0;

char*                                        ProjectName;
char                                         GasSwapFileName[255];
char                                         ErrFileName[255];
char                                         OutFileName[255];
char                                         TecPlotFileName[255];
int                                          fd_g;
void*                                        GasSwapData;
int                                          TurbMod    = 0;
int                                          EndFLAG    = 1;
int                                          PrintFLAG;
ofstream*                                    pInputData;        // Output data stream
ofstream*                                    pHeatFlux_OutFile; // Output HeatFlux stream
unsigned int                                 iter = 0;          // iteration number
unsigned int                                 last_iter=0;       // Global iteration number
int                                          isStop=0;          // Stop flag
InputData*                                   Data=NULL;         // Object data loader
UMatrix2D< FlowNode2D<FP,NUM_COMPONENTS> >*  J=NULL;        // Main computation area
UMatrix2D< FlowNodeCore2D<FP,NUM_COMPONENTS> >*  C=NULL;    // Main computation area
UArray<Flow*>*                               FlowList=NULL;     // List of 'Flow' objects
UArray<Flow2D*>*                             Flow2DList=NULL;   // List of 'Flow2D' objects
UArray<Bound2D*>                             SingleBoundsList;  // Single Bounds List;

FP                                           dt;                // time step
FP*                                          RoUx=NULL;
FP*                                          RoVy=NULL;
FP                                           GlobalTime=0.;
FP                                           CurrentTimePart=0;

ofstream*                                    pOutputData;     // output data stream (file)

int                                          I,NSaveStep;
unsigned int                                 MaxX=0;          // X dimension of computation area
unsigned int                                 MaxY=0;          // Y dimension of computation area

FP                                           dxdy,dx2,dy2;
FP                                           SigW,SigF,delta_bl;

unsigned long                                FileSizeGas      = 0;
int                                          isVerboseOutput       = 0;
int                                          isTurbulenceReset     = 0;

int SetNonReflectedBC(UMatrix2D< FlowNode2D<FP,NUM_COMPONENTS> >* OutputMatrix2D,
                      FP beta_nrbc,
                      ofstream* f_stream) {
    int nr_nodes = 0;
// Clean walls nodes
#ifdef _OPEN_MP
#pragma omp parallel for \
        reduction(+:nr_nodes)
#endif //_OPEN_MP

    for ( unsigned int ii=0;ii<OutputMatrix2D->GetX();ii++ ) {
        for ( unsigned  int jj=0;jj<OutputMatrix2D->GetY();jj++ ) {
                if ( OutputMatrix2D->GetValue(ii,jj).isCond2D(NT_FARFIELD_2D) ) {
                    nr_nodes++;
                    if ( ii > 0 &&
                         OutputMatrix2D->GetValue(ii-1,jj).isCond2D(CT_NODE_IS_SET_2D) &&
                         !OutputMatrix2D->GetValue(ii-1,jj).isCond2D(CT_WALL_NO_SLIP_2D) &&
                         !OutputMatrix2D->GetValue(ii-1,jj).isCond2D(CT_SOLID_2D) &&
                         !OutputMatrix2D->GetValue(ii-1,jj).isCond2D(NT_FC_2D) ) {

                        OutputMatrix2D->GetValue(ii-1,jj).SetCond2D(CT_NONREFLECTED_2D);
                        nr_nodes++;
                    }
                    if ( ii < OutputMatrix2D->GetX() - 1 &&
                         OutputMatrix2D->GetValue(ii+1,jj).isCond2D(CT_NODE_IS_SET_2D) &&
                         !OutputMatrix2D->GetValue(ii+1,jj).isCond2D(CT_WALL_NO_SLIP_2D) &&
                         !OutputMatrix2D->GetValue(ii+1,jj).isCond2D(CT_SOLID_2D) &&
                         !OutputMatrix2D->GetValue(ii+1,jj).isCond2D(NT_FC_2D) ) {

                        OutputMatrix2D->GetValue(ii+1,jj).SetCond2D(CT_NONREFLECTED_2D);
                        nr_nodes++;
                    }
                    if ( jj > 0 &&
                         OutputMatrix2D->GetValue(ii,jj-1).isCond2D(CT_NODE_IS_SET_2D) &&
                         !OutputMatrix2D->GetValue(ii,jj-1).isCond2D(CT_WALL_NO_SLIP_2D) &&
                         !OutputMatrix2D->GetValue(ii,jj-1).isCond2D(CT_SOLID_2D) &&
                         !OutputMatrix2D->GetValue(ii,jj-1).isCond2D(NT_FC_2D) ) {

                        OutputMatrix2D->GetValue(ii,jj-1).SetCond2D(CT_NONREFLECTED_2D);
                        nr_nodes++;
                    }
                    if ( jj < OutputMatrix2D->GetY() - 1 &&
                         OutputMatrix2D->GetValue(ii,jj+1).isCond2D(CT_NODE_IS_SET_2D) &&
                         !OutputMatrix2D->GetValue(ii,jj+1).isCond2D(CT_WALL_NO_SLIP_2D) &&
                         !OutputMatrix2D->GetValue(ii,jj+1).isCond2D(CT_SOLID_2D) &&
                         !OutputMatrix2D->GetValue(ii,jj+1).isCond2D(NT_FC_2D) ) {

                        OutputMatrix2D->GetValue(ii,jj+1).SetCond2D(CT_NONREFLECTED_2D);
                        nr_nodes++;
                    }
                }
            }
        }
    return nr_nodes;
}


/*  Init -DEEPS2D- solver */
void InitSharedData(InputData* _data,
                    void* CRM_data
#ifdef _MPI
                    ,int rank
#endif //_MPI
                    ) {
            ChemicalReactionsModelData2D* model_data = (ChemicalReactionsModelData2D*)CRM_data;

            fd_g=0;
            GasSwapData         = NULL;
            TurbMod             = 0;
            
            ThreadBlockSize = _data->GetIntVal((char*)"ThreadBlockSize");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            isSingleGPU = _data->GetIntVal((char*)"isSingleGPU");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
           
            if(isSingleGPU) {
                ActiveSingleGPU = _data->GetIntVal((char*)"ActiveSingleGPU");
                if ( _data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
            }
            
            isIgnoreUnsetNodes = _data->GetIntVal((char*)"isIgnoreUnsetNodes");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            isVerboseOutput = _data->GetIntVal((char*)"isVerboseOutput");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            
            bFF   = (BlendingFactorFunction)_data->GetIntVal((char*)"BFF");  // Blending factor function type
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            
            MaxX      = Data->GetIntVal((char*)"MaxX");          // X dimension of computation area
            
            if ( Data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            MaxY      = Data->GetIntVal((char*)"MaxY");          // Y dimension of computation area
            if ( Data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            dx        = Data->GetFloatVal((char*)"dx");          // x size of cell 
            if ( Data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            dy        = Data->GetFloatVal((char*)"dy");          // y size of sell
            if ( Data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
#ifdef _MPI
            if(rank==0) {
#endif //_MPI
                *(_data->GetMessageStream()) << "X=" << MaxX << "  Y=" << MaxY << "  dx=" << dx << "  dy=" << dy <<"\n" << flush;
                _data->GetMessageStream()->flush();
#ifdef _MPI
            }
#endif //_MPI
            SigW     = _data->GetFloatVal((char*)"SigW");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            SigF     = _data->GetFloatVal((char*)"SigF");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            delta_bl  = _data->GetFloatVal((char*)"delta_bl");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            TurbMod  = _data->GetIntVal((char*)"TurbulenceModel");
            if ( _data->GetDataError()==-1 ) {
                 Abort_OpenHyperFLOW2D();
            }

            TurbStartIter  = _data->GetIntVal((char*)"TurbStartIter");
            if ( _data->GetDataError()==-1 ) {
                 Abort_OpenHyperFLOW2D();
            }

            TurbExtModel = _data->GetIntVal((char*)"TurbExtModel");
            if ( _data->GetDataError()==-1 ) {
                 Abort_OpenHyperFLOW2D();
            }

            isTurbulenceReset = _data->GetIntVal((char*)"isTurbulenceReset");
            if ( _data->GetDataError()==-1 ) {
                 Abort_OpenHyperFLOW2D();
            }

            FlowNode2D<FP,NUM_COMPONENTS>::FT = (FlowType)(_data->GetIntVal((char*)"FlowType"));
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            ProblemType = (SolverMode)(_data->GetIntVal((char*)"ProblemType"));
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            /*
            isRecalcYplus  = _data->GetIntVal((char*)"isRecalcYplus");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            isHighOrder = _data->GetIntVal((char*)"isHighOrder");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            */

            CFL0  = _data->GetFloatVal((char*)"CFL");               // Courant number
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            CFL_Scenario  = _data->GetTable((char*)"CFL_Scenario"); // Courant number scenario
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            isLocalTimeStep = _data->GetIntVal((char*)"isLocalTimeStep");
            if ( _data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
#ifdef   _RMS_
            isAlternateRMS = _data->GetIntVal((char*)"isAlternateRMS");
            if ( _data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
#endif //_RMS_ 

            NSaveStep = _data->GetIntVal((char*)"NSaveStep");
            if ( _data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

            Nstep           = _data->GetIntVal((char*)"Nmax");     // Sync computation area each after NMax iterations  
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            NOutStep  = _data->GetIntVal((char*)"NOutStep");       // Output step
            if ( NOutStep<=0 )     NOutStep=1;

            if(NOutStep >= Nstep)
               Nstep = NOutStep + 1;

            MonitorIndex = _data->GetIntVal((char*)"MonitorIndex");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            if(MonitorIndex > 5 ||
               MonitorIndex < 0) {
               MonitorIndex = 0;
            }

            ExitMonitorValue  = _data->GetFloatVal((char*)"ExitMonitorValue");   // Monitor value for exit
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            int NumMonitorPoints = _data->GetIntVal((char*)"NumMonitorPoints");
            
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
// Make output variables list            
            char* OutVariablesList = _data->GetStringVal((char*)"OutVariablesList");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            } else {
                OutVariable TmpOutVar;
                char* VarName;
                OutVariables_Array = new UArray<OutVariable>();
                
                VarName = strtok(OutVariablesList,",");
                
                if (!VarName) {
                    strncpy(TmpOutVar.VarName,OutVariablesList,64);
                    TmpOutVar.VarID = FindVarName(TmpOutVar.VarName);
                    if (TmpOutVar.VarID == UNKNOWN_ID) {
                        *(_data->GetMessageStream()) << "Unknown variable \"" << TmpOutVar.VarName  << "\" ignored." << endl;
                    } else {
                        *(_data->GetMessageStream()) << "Add variable \"" << TmpOutVar.VarName  << "\" to out variables list..." << endl;
                        OutVariables_Array->AddElement(&TmpOutVar);
                    }

                } else {
                    do {
                         
                         strncpy(TmpOutVar.VarName,VarName,64);
                         TmpOutVar.VarID = FindVarName(TmpOutVar.VarName);
                         if (TmpOutVar.VarID == UNKNOWN_ID) {
                             *(_data->GetMessageStream()) << "Unknown variable \"" << TmpOutVar.VarName  << "\" ignored." << endl;
                         } else {
                             *(_data->GetMessageStream()) << "Add variable \"" << TmpOutVar.VarName  << "\" to output variables list." << endl;
                             OutVariables_Array->AddElement(&TmpOutVar);
                         }
                         VarName = strtok(NULL,",");
                    }while(VarName);

                } 
              if (OutVariables_Array->GetNumElements() == 0) {
               *(_data->GetMessageStream()) << "ERROR: Output variables list is empthy !" << endl;
                Abort_OpenHyperFLOW2D();
              }
            }

            if (NumMonitorPoints > 0 ) {
                *(_data->GetMessageStream()) << "Read monitor points...\n" << flush;
                MonitorPointsArray = new UArray< MonitorPoint >(); 
                MonitorPoint TmpPoint;
                for (int i=0;i<NumMonitorPoints;i++) {
                   char   point_str[256];
                   FP point_xy;
                   snprintf(point_str,256,"Point-%i.X",i+1);
                   point_xy =  _data->GetFloatVal(point_str);
                   if ( _data->GetDataError()==-1 ) {
                       Abort_OpenHyperFLOW2D();
                   }
                   TmpPoint.MonitorXY.SetX(point_xy);
                   snprintf(point_str,256,"Point-%i.Y",i+1);
                   point_xy =  _data->GetFloatVal(point_str);
                   if ( _data->GetDataError()==-1 ) {
                       Abort_OpenHyperFLOW2D();
                   }
                   
                   TmpPoint.MonitorXY.SetY(point_xy);

                   if(TmpPoint.MonitorXY.GetX() < 0.0 ||
                      TmpPoint.MonitorXY.GetY() < 0.0 ||
                      TmpPoint.MonitorXY.GetX() > MaxX*dx ||
                      TmpPoint.MonitorXY.GetY() > MaxY*dy ) {
                      *(_data->GetMessageStream()) << "Point no " << i+1 << " X=" << TmpPoint.MonitorXY.GetX() << "m Y=" << TmpPoint.MonitorXY.GetY() << " m out of domain...monitor ignored." << endl;
                   } else {
                       MonitorPointsArray->AddElement(&TmpPoint);
                    *(_data->GetMessageStream()) << "Point no " << i+1 << " X=" << TmpPoint.MonitorXY.GetX() << "m Y=" << TmpPoint.MonitorXY.GetY() << " m" << endl;
                }
               }
                *(_data->GetMessageStream()) << "Load " << NumMonitorPoints << " monitor points...OK" << endl;
            }
            
            beta0  = _data->GetFloatVal((char*)"beta");                         // Base blending factor.
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            
            nrbc_beta0 = _data->GetFloatVal((char*)"beta_NonReflectedBC");      // Base blending factor for non-reflected BC.
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            beta_Scenario  = _data->GetTable((char*)"beta_Scenario");           //  Base blending factor scenario
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            
            
            // Model of chemical reactions
            // 0 - No reactions
            // 1 - Zeldovich model
            // 2 - Arrenius model
            
            Chemical_reactions_model = _data->GetIntVal("ChemicalReactionsModel"); // Model of chemical reactions
            if ( _data->GetDataError()==-1 ) {
               Abort_OpenHyperFLOW2D();
            }

            if (Chemical_reactions_model > 0) {
                model_data->K0     = _data->GetFloatVal((char*)"K0"); // Stoichiometric ratio (OX/Fuel)
                if ( _data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }

                model_data->gamma  = _data->GetFloatVal((char*)"gamma"); // Factor completion of a chemical reaction
                if ( _data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }

                model_data->Tf     = _data->GetFloatVal((char*)"Tf");   // Ignition temperature
                if ( _data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
            }

            isAdiabaticWall     = _data->GetIntVal((char*)"isAdiabaticWall");  // is walls adiabatic ?
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

/* Load combustion products properties */

            model_data->R_cp   = _data->GetFloatVal((char*)"R_cp");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->H_cp   = _data->GetFloatVal((char*)"H_cp");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->lam_cp  = _data->GetTable((char*)"lam_cp");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->mu_cp   = _data->GetTable((char*)"mu_cp");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->Cp_cp   = _data->GetTable((char*)"Cp_cp");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

/* Load fuel properties */

            model_data->R_Fuel = _data->GetFloatVal((char*)"R_Fuel");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->H_Fuel = _data->GetFloatVal((char*)"H_Fuel");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->lam_Fuel     = _data->GetTable((char*)"lam_Fuel");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->mu_Fuel      = _data->GetTable((char*)"mu_Fuel");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->Cp_Fuel      = _data->GetTable((char*)"Cp_Fuel");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

/* Load OX properties */

            model_data->R_OX   = _data->GetFloatVal((char*)"R_OX");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->H_OX   = _data->GetFloatVal((char*)"H_OX");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->lam_OX       = _data->GetTable((char*)"lam_OX");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->mu_OX        = _data->GetTable((char*)"mu_OX");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->Cp_OX        = _data->GetTable((char*)"Cp_OX");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

/* Load air properties */

            model_data->R_air   = _data->GetFloatVal((char*)"R_air");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->H_air   = _data->GetFloatVal((char*)"H_air");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->lam_air       = _data->GetTable((char*)"lam_air");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->mu_air        = _data->GetTable((char*)"mu_air");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            model_data->Cp_air        = _data->GetTable((char*)"Cp_air");
            if ( _data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

#ifdef _UNIFORM_MESH_
            FlowNode2D<FP,NUM_COMPONENTS>::dx        = dx;
            FlowNode2D<FP,NUM_COMPONENTS>::dy        = dy;
#endif //_UNIFORM_MESH_
            FlowNode2D<FP,NUM_COMPONENTS>::Hu[h_fu]  = model_data->H_Fuel;
            FlowNode2D<FP,NUM_COMPONENTS>::Hu[h_ox]  = model_data->H_OX;
            FlowNode2D<FP,NUM_COMPONENTS>::Hu[h_cp]  = model_data->H_cp;
            FlowNode2D<FP,NUM_COMPONENTS>::Hu[h_air] = model_data->H_air;
};


void* InitDEEPS2D(void* lpvParam)
    {
        unsigned int    i_last=0,i_err=0,j_err=0;
        unsigned int    k,j=0;
        ofstream*       f_stream=(ofstream*)lpvParam;
        char            FlowStr[256];
        Flow*           TmpFlow;
        Flow2D*         TmpFlow2D;
        Flow*           pTestFlow=NULL;
        Flow2D*         pTestFlow2D=NULL;
        char            NameBound[128];
        char            NameContour[128];
        char*           BoundStr;
        int             FlowIndex;
        Table*          ContourTable;
        CondType2D      TmpCT = CT_NO_COND_2D;
        TurbulenceCondType2D TmpTurbulenceCT = TCT_No_Turbulence_2D;
        BoundContour2D*   BC; 
        char            ErrorMessage[255];
        u_long          FileSizeGas=0;
        static int      PreloadFlag = 0,p_g=0;
        FP              chord = 0;

        SubDomainArray     = new UArray<UMatrix2D< FlowNode2D<FP,NUM_COMPONENTS> >* >();
        CoreSubDomainArray = new UArray<UMatrix2D< FlowNodeCore2D<FP,NUM_COMPONENTS> >* >();
#ifdef _DEBUG_0
        ___try {
#endif  // _DEBUG_0

            unsigned int NumFlow = Data->GetIntVal((char*)"NumFlow");       // Number of "Flow" objects (for additional information see libFlow library source)
            if ( Data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            unsigned int NumFlow2D = Data->GetIntVal((char*)"NumFlow2D");   // Number of "Flow2D" objects (for additional information see libFlow library source)
            if ( Data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
            unsigned int NumArea = Data->GetIntVal((char*)"NumArea");       // Number of "Area" objects 
            if ( Data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }

            NumContour = Data->GetIntVal((char*)"NumContour"); // Number of "Contour" objects 
            if ( Data->GetDataError()==-1 ) {
                Abort_OpenHyperFLOW2D();
            }
//---------------------    File Names   ------------------------------
            ProjectName = Data->GetStringVal((char*)"ProjectName");         // Project Name
            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
            sprintf(GasSwapFileName,"%s%s",ProjectName,Data->GetStringVal((char*)"GasSwapFile")) ; // Swap File name for gas area...
            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
            sprintf(OutFileName,"%s%s",ProjectName,Data->GetStringVal((char*)"OutputFile"));
            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
            sprintf(TecPlotFileName,"tp-%s",OutFileName);
            sprintf(ErrFileName,"%s%s",ProjectName,Data->GetStringVal((char*)"ErrorFile"));
            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
//-----------------------------------------------------------------------
            Ts0 = Data->GetFloatVal((char*)"Ts0");
            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

            isOutHeatFluxX = Data->GetIntVal((char*)"isOutHeatFluxX");
            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
            
            if(isOutHeatFluxX) { 
                Cp_Flow_index = Data->GetIntVal((char*)"Cp_Flow_Index");
                if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();
                y_max = Data->GetIntVal((char*)"y_max");
                if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();
                y_min = Data->GetIntVal((char*)"y_min");
                if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();
            }

            isOutHeatFluxY = Data->GetIntVal((char*)"isOutHeatFluxY");
            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

            // Clear Flow list
            if ( FlowList ) {
                for (int i=0;i<(int)FlowList->GetNumElements();i++ ) {
                    delete  FlowList->GetElement(i);
                }
                delete FlowList;
                FlowList=NULL;
            }

            // Load Flow list
            FlowList=new UArray<Flow*>();
            for (int i=0;i<(int)NumFlow;i++ ) {
                snprintf(FlowStr,256,"Flow%i.p",i+1);
                Pg = Data->GetFloatVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
                snprintf(FlowStr,256,"Flow%i.T",i+1);
                Tg = Data->GetFloatVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
                snprintf(FlowStr,256,"Flow%i.CompIndex",i+1);
                CompIndex = Data->GetIntVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                     Abort_OpenHyperFLOW2D();
                }
                    if ( CompIndex==0 ) {   // Fuel
                        Cp =chemical_reactions.Cp_Fuel->GetVal(Tg);
                        lam=chemical_reactions.lam_Fuel->GetVal(Tg);
                        mu =chemical_reactions.mu_Fuel->GetVal(Tg);
                        Rg =chemical_reactions.R_Fuel;
                    } else if ( CompIndex==1 ) {  // ox
                        Cp =chemical_reactions.Cp_OX->GetVal(Tg);
                        lam=chemical_reactions.lam_OX->GetVal(Tg);
                        mu =chemical_reactions.mu_OX->GetVal(Tg);
                        Rg =chemical_reactions.R_OX;
                    } else if ( CompIndex==2 ) {  // Combustion products
                        Cp =chemical_reactions.Cp_cp->GetVal(Tg);
                        lam=chemical_reactions.lam_cp->GetVal(Tg);
                        mu =chemical_reactions.mu_cp->GetVal(Tg);
                        Rg =chemical_reactions.R_cp;
                    } else if ( CompIndex==3 ) {  // Air
                        Cp =chemical_reactions.Cp_air->GetVal(Tg);
                        lam=chemical_reactions.lam_air->GetVal(Tg);
                        mu =chemical_reactions.mu_air->GetVal(Tg);
                        Rg =chemical_reactions.R_air;
                    }  else if ( CompIndex==4 ) {  // Mixture
                       snprintf(FlowStr,256,"Flow%i.Y_fuel",i+1);
                       if ( Data->GetDataError()==-1 ) {
                            Abort_OpenHyperFLOW2D();
                       } 
                       Y_mix[0] =  Data->GetFloatVal(FlowStr);
                       snprintf(FlowStr,256,"Flow%i.Y_ox",i+1);
                       if ( Data->GetDataError()==-1 ) {
                            Abort_OpenHyperFLOW2D();
                       } 
                       Y_mix[1] =  Data->GetFloatVal(FlowStr);
                       snprintf(FlowStr,256,"Flow%i.Y_cp",i+1);
                       if ( Data->GetDataError()==-1 ) {
                            Abort_OpenHyperFLOW2D();
                       } 
                       Y_mix[2] =  Data->GetFloatVal(FlowStr);
                       snprintf(FlowStr,256,"Flow%i.Y_air",i+1);
                       if ( Data->GetDataError()==-1 ) {
                            Abort_OpenHyperFLOW2D();
                       } 
                       Y_mix[3] = 1 - Y_mix[0] + Y_mix[1] + Y_mix[2];
                       Cp = Y_mix[0]*chemical_reactions.Cp_Fuel->GetVal(Tg) + Y_mix[1]*chemical_reactions.Cp_OX->GetVal(Tg) + Y_mix[2]*chemical_reactions.Cp_cp->GetVal(Tg)+Y_mix[3]*chemical_reactions.Cp_air->GetVal(Tg);
                       lam= Y_mix[0]*chemical_reactions.lam_Fuel->GetVal(Tg)+ Y_mix[1]*chemical_reactions.lam_OX->GetVal(Tg)+ Y_mix[2]*chemical_reactions.lam_cp->GetVal(Tg)+Y_mix[3]*chemical_reactions.lam_air->GetVal(Tg);
                       mu = Y_mix[0]*chemical_reactions.mu_Fuel->GetVal(Tg) + Y_mix[1]*chemical_reactions.mu_OX->GetVal(Tg) + Y_mix[2]*chemical_reactions.mu_cp->GetVal(Tg)+Y_mix[3]*chemical_reactions.mu_air->GetVal(Tg);
                       Rg = Y_mix[0]*chemical_reactions.R_Fuel + Y_mix[1]*chemical_reactions.R_OX + Y_mix[2]*chemical_reactions.R_cp+Y_mix[3]*chemical_reactions.R_air;
                    } else {
                    *f_stream << "\n";
                    *f_stream << "Bad component index \""<< CompIndex <<"\" use in Flow"<< i+1 <<"\n" << flush;
                    f_stream->flush();
                    Abort_OpenHyperFLOW2D();
                }

                TmpFlow = new Flow(Cp,Tg,Pg,Rg,lam,mu);

                snprintf(FlowStr,256,"Flow%i.Type",i+1);
                FP  LamF;
                int F_Type  = Data->GetIntVal(FlowStr);
                if ( (int)Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
                if ( F_Type == 0 ) {
                    snprintf(FlowStr,256,"Flow%i.Lam",i+1);
                    LamF = Data->GetFloatVal(FlowStr);
                    if ( Data->GetDataError()==-1 ) {
                        Abort_OpenHyperFLOW2D();
                    }
                    TmpFlow->LAM(LamF);
                } else {
                    snprintf(FlowStr,256,"Flow%i.W",i+1);
                    Wg = Data->GetFloatVal(FlowStr);
                    if ( Data->GetDataError()==-1 ) {
                        Abort_OpenHyperFLOW2D();
                    }
                    TmpFlow->Wg(Wg);
                }
                FlowList->AddElement(&TmpFlow);
                *f_stream << "Add object \"Flow" << i+1 <<  "\"...OK\n" << flush;
                f_stream->flush();
            }

            // Clear Flow2D list
            if ( Flow2DList ) {
                for (int i=0;i<(int)Flow2DList->GetNumElements();i++ ) {
                    delete  Flow2DList->GetElement(i);
                }
                delete Flow2DList;
                Flow2DList=NULL;
            }

            // Load Flow2D list
            Flow2DList=new UArray<Flow2D*>();
            for (int i=0;i<(int)NumFlow2D;i++ ) {
                snprintf(FlowStr,256,"Flow2D-%i.CompIndex",i+1);
                CompIndex = Data->GetIntVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }

                snprintf(FlowStr,256,"Flow2D-%i.p",i+1);
                Pg = Data->GetFloatVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
                snprintf(FlowStr,256,"Flow2D-%i.T",i+1);
                Tg = Data->GetFloatVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }

                if ( CompIndex==0 ) {   // Fuel
                    Cp =chemical_reactions.Cp_Fuel->GetVal(Tg);
                    lam=chemical_reactions.lam_Fuel->GetVal(Tg);
                    mu =chemical_reactions.mu_Fuel->GetVal(Tg);
                    Rg =chemical_reactions.R_Fuel;
                } else if ( CompIndex==1 ) {  // OX
                    Cp =chemical_reactions.Cp_OX->GetVal(Tg);
                    lam=chemical_reactions.lam_OX->GetVal(Tg);
                    mu =chemical_reactions.mu_OX->GetVal(Tg);
                    Rg =chemical_reactions.R_OX;
                } else if ( CompIndex==2 ) {  // Combustion products
                    Cp =chemical_reactions.Cp_cp->GetVal(Tg);
                    lam=chemical_reactions.lam_cp->GetVal(Tg);
                    mu =chemical_reactions.mu_cp->GetVal(Tg);
                    Rg =chemical_reactions.R_cp;
                } else if ( CompIndex==3 ) {  // Air
                    Cp =chemical_reactions.Cp_air->GetVal(Tg);
                    lam=chemical_reactions.lam_air->GetVal(Tg);
                    mu =chemical_reactions.mu_air->GetVal(Tg);
                    Rg =chemical_reactions.R_air;
                }  else if ( CompIndex==4 ) {  // Mixture
                    snprintf(FlowStr,256,"Flow2D-%i.Y_fuel",i+1);
                    if ( Data->GetDataError()==-1 ) {
                         Abort_OpenHyperFLOW2D();
                    } 
                    Y_mix[0] =  Data->GetFloatVal(FlowStr);
                    snprintf(FlowStr,256,"Flow2D-%i.Y_ox",i+1);
                    if ( Data->GetDataError()==-1 ) {
                         Abort_OpenHyperFLOW2D();
                    } 
                    Y_mix[1] =  Data->GetFloatVal(FlowStr);
                    snprintf(FlowStr,256,"Flow2D-%i.Y_cp",i+1);
                    if ( Data->GetDataError()==-1 ) {
                         Abort_OpenHyperFLOW2D();
                    } 
                    Y_mix[2] =  Data->GetFloatVal(FlowStr);
                    snprintf(FlowStr,256,"Flow2D-%i.Y_air",i+1);
                    if ( Data->GetDataError()==-1 ) {
                         Abort_OpenHyperFLOW2D();
                    }
                    Y_mix[3] = 1 - Y_mix[0] + Y_mix[1] + Y_mix[2];
                    Cp = Y_mix[0]*chemical_reactions.Cp_Fuel->GetVal(Tg) + Y_mix[1]*chemical_reactions.Cp_OX->GetVal(Tg) + Y_mix[2]*chemical_reactions.Cp_cp->GetVal(Tg)+Y_mix[3]*chemical_reactions.Cp_air->GetVal(Tg);
                    lam= Y_mix[0]*chemical_reactions.lam_Fuel->GetVal(Tg)+ Y_mix[1]*chemical_reactions.lam_OX->GetVal(Tg)+ Y_mix[2]*chemical_reactions.lam_cp->GetVal(Tg)+Y_mix[3]*chemical_reactions.lam_air->GetVal(Tg);
                    mu = Y_mix[0]*chemical_reactions.mu_Fuel->GetVal(Tg) + Y_mix[1]*chemical_reactions.mu_OX->GetVal(Tg) + Y_mix[2]*chemical_reactions.mu_cp->GetVal(Tg)+Y_mix[3]*chemical_reactions.mu_air->GetVal(Tg);
                    Rg = Y_mix[0]*chemical_reactions.R_Fuel + Y_mix[1]*chemical_reactions.R_OX + Y_mix[2]*chemical_reactions.R_cp + Y_mix[3]*chemical_reactions.R_air;
                }else {
                    *f_stream << "\n";
                    *f_stream << "Bad component index \""<< CompIndex <<"\" use in Flow2D-"<< i+1 <<"\n" << flush;
                    f_stream->flush();
                    Abort_OpenHyperFLOW2D();
                }
                snprintf(FlowStr,256,"Flow2D-%i.U",i+1);
                Ug = Data->GetFloatVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
                snprintf(FlowStr,256,"Flow2D-%i.V",i+1);
                Vg = Data->GetFloatVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
                
                snprintf(FlowStr,256,"Flow2D-%i.Mode",i+1); 
                int FlowMode = Data->GetIntVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
                // 0 - U + V + static p, T
                // 1 - U + V + total p*, T*
                // 2 - Mach + Angle + static p*, T*
                // 3 - Mach + Angle + total p, T

                if(FlowMode == 2) {
                  Ug = Vg = 0;
                }

                TmpFlow2D = new Flow2D(mu,lam,Cp,Tg,Pg,Rg,Ug,Vg);
                
                //*f_stream << "mu=" << TmpFlow2D->mu << endl;
                //*f_stream << "lam=" << TmpFlow2D->lam << endl;

                if(FlowMode == 0) {
                    TmpFlow2D->CorrectFlow(Tg,Pg,sqrt(Ug*Ug+Vg*Vg+1.e-30),FV_VELOCITY);

                } if(FlowMode == 2 || FlowMode == 3) {
                    snprintf(FlowStr,256,"Flow2D-%i.Mach",i+1);
                    FP Mach = Data->GetFloatVal(FlowStr);
                    if ( Data->GetDataError()==-1 ) {
                        Abort_OpenHyperFLOW2D();
                    }

                    snprintf(FlowStr,256,"Flow2D-%i.Angle",i+1); // Angle (deg.)
                    FP Angle = Data->GetFloatVal(FlowStr);
                    if ( Data->GetDataError()==-1 ) {
                        Abort_OpenHyperFLOW2D();
                    }

                    if(FlowMode == 2 ) {
                       TmpFlow2D->CorrectFlow(Tg,Pg,Mach);
                    }

                    TmpFlow2D->MACH(Mach);

                    Wg = TmpFlow2D->Wg();
                    Ug = cos(Angle*M_PI/180)*Wg; 
                    Vg = sin(Angle*M_PI/180)*Wg;
                    TmpFlow2D->Wg(Ug,Vg);
                }

                Flow2DList->AddElement(&TmpFlow2D);
                *f_stream << "Add object \"Flow2D-" << i+1 << " Mach=" << TmpFlow2D->MACH()
                                                           << " U="    << TmpFlow2D->U()   << " m/sec"
                                                           << " V="    << TmpFlow2D->V()   << " m/sec"
                                                           << " Wg="   << TmpFlow2D->Wg()  << " m/sec"
                                                           << " T="    << TmpFlow2D->Tg()  << " K"
                                                           << " p="    << TmpFlow2D->Pg()  << " Pa"
                                                           << " p*="   << TmpFlow2D->P0()  << " Pa"
                                                           << " T*="   << TmpFlow2D->T0()  << " K\"...OK\n" << flush;
                f_stream->flush(); 
            }

            // Load X cuts list
             XCutArray   = new UArray<XCut>();
             XCut        TmpXCut;
             unsigned int NumXCut = Data->GetIntVal((char*)"NumXCut");
             if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
             }

            for (int i=0;i<(int)NumXCut;i++ ) {
                snprintf(FlowStr,256,"CutX-%i.x0",i+1);
                TmpXCut.x0 = Data->GetFloatVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }

                snprintf(FlowStr,256,"CutX-%i.y0",i+1);
                TmpXCut.y0 = Data->GetFloatVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }

                snprintf(FlowStr,256,"CutX-%i.dy",i+1);
                TmpXCut.dy = Data->GetFloatVal(FlowStr);
                if ( Data->GetDataError()==-1 ) {
                    Abort_OpenHyperFLOW2D();
                }
                *f_stream << "Add test XCut No" << i+1 << " X=" << TmpXCut.x0 << " Y=" << TmpXCut.y0 << " dY=" << TmpXCut.dy;
                XCutArray->AddElement(&TmpXCut);
                *f_stream << "...OK\n" << flush;
            }

            FileSizeGas =  MaxX*MaxY*sizeof(FlowNode2D<FP,NUM_COMPONENTS>);
            

            GasSwapData   = LoadSwapFile2D(GasSwapFileName,
                                           (int)MaxX,
                                           (int)MaxY,
                                           sizeof(FlowNode2D<FP,NUM_COMPONENTS>),
                                           &p_g,
                                           &fd_g,
                                           f_stream);
            PreloadFlag = p_g;

            if ( GasSwapFileName!=NULL )
                if ( GasSwapData == 0 ) {
                    PreloadFlag = 0;
                    *f_stream << "\n";
                    *f_stream << "Error mapping swap file...Start without swap file.\n" << flush;
                    f_stream->flush();
                    if ( GasSwapData != 0 ) {
                        munmap(GasSwapData,FileSizeGas);
                        close(fd_g);
                        unlink(GasSwapFileName);
                    }
                }
#ifdef _DEBUG_0
            ___try {
#endif  // _DEBUG_0
                if ( GasSwapData!=0 ) {
                    *f_stream << "Mapping computation area..." << flush;
                    f_stream->flush();
                    J = new UMatrix2D< FlowNode2D<FP,NUM_COMPONENTS> >((FlowNode2D<FP,NUM_COMPONENTS>*)GasSwapData,MaxX,MaxY);
                    C = new UMatrix2D< FlowNodeCore2D<FP,NUM_COMPONENTS> >(MaxX,MaxY);
                    useSwapFile=1;
                    sprintf(OldSwapFileName,"%s",GasSwapFileName);
                    OldSwapData = GasSwapData;
                    OldFileSizeGas = FileSizeGas;
                    Old_fd = fd_g;
                } else {
                    *f_stream << "Allocate computation area..." << flush;
                    f_stream->flush();
                    if ( J ) {
                        delete J;J=NULL;
                    }
                    J = new UMatrix2D< FlowNode2D<FP,NUM_COMPONENTS> >(MaxX,MaxY);
                }
#ifdef _DEBUG_0
            } __except( ComputationalMatrix2D*  m) {  // ExceptLib know bug...
#endif  // _DEBUG_0
                if ( J->GetMatrixState()!=MXS_OK ) {
                    *f_stream << "\n";
                    *f_stream << " Memory allocation error !\n" << flush;
                    f_stream->flush();
                    Abort_OpenHyperFLOW2D();
                }
#ifdef _DEBUG_0
            } __end_except;
#endif  // _DEBUG_0

            *f_stream << "OK\n" << flush;
            f_stream->flush();

            dt=1.;
            for (int i = 0;i<(int)FlowList->GetNumElements();i++ ) {
                 FP CFL_min      = min(CFL0,CFL_Scenario->GetVal(iter+last_iter));
                 dt = min(dt,CFL_min*dx*dy/(dy*(FlowList->GetElement(i)->Asound()*2.)+
                                            dx*FlowList->GetElement(i)->Asound()));
            }

            sprintf(ErrorMessage, "\nComputation terminated.\nInternal error in OpenHyperFLOW2D/DEEPS.\n");
            static int   isFlow2D=0;
            static int   is_reset;
            unsigned int ix,iy;

#ifdef _DEBUG_0
            ___try {
#endif // _DEBUG_0

 /* Start SingleBound loading */
 //if(!PreloadFlag) {
   Table*       SingleBoundTable;
   Bound2D*     SingleBound;
   static int   BoundMaterialID;
   unsigned int s_x,s_y,e_x,e_y;
   unsigned int numSingleBounds=Data->GetIntVal((char*)"NumSingleBounds");
                if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();
                    for (int i=1;i<(int)numSingleBounds+1;i++ ) {
                            sprintf(NameContour,"SingleBound%i",i);
                            sprintf(NameBound,"%s.Points",NameContour);
                            SingleBoundTable = Data->GetTable(NameBound);
                            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                            s_x = max((unsigned int)(SingleBoundTable->GetX(0)/dx),0);
                            s_y = max((unsigned int)(SingleBoundTable->GetY(0)/dy),0);
                            e_x = max((unsigned int)(SingleBoundTable->GetX(1)/dx),0);
                            e_y = max((unsigned int)(SingleBoundTable->GetY(1)/dy),0);

                            sprintf(NameBound,"%s.Cond",NameContour);
                            BoundStr = Data->GetStringVal(NameBound);
                            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                            sprintf(NameBound,"%s.TurbulenceModel",NameContour);
                            int TurbMod=Data->GetIntVal(NameBound);
                            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                            TmpCT           = CT_NO_COND_2D;
                            TmpTurbulenceCT = TCT_No_Turbulence_2D;

                            if ( TurbMod == 0 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_No_Turbulence_2D);
                            else if ( TurbMod == 1 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Integral_Model_2D);
                            else if ( TurbMod == 2 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Prandtl_Model_2D);
                            else if ( TurbMod == 3 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Spalart_Allmaras_Model_2D);
                            else if ( TurbMod == 4 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_k_eps_Model_2D);
                            else if ( TurbMod == 5 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Smagorinsky_Model_2D);

                            // Atomic conditions
                            if ( strstr(BoundStr,"CT_Rho_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_Rho_CONST_2D);
                            if ( strstr(BoundStr,"CT_U_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_U_CONST_2D);
                            if ( strstr(BoundStr,"CT_V_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_V_CONST_2D);
                            if ( strstr(BoundStr,"CT_T_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_T_CONST_2D);
                            if ( strstr(BoundStr,"CT_Y_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_Y_CONST_2D);
                            if ( strstr(BoundStr,"CT_WALL_LAW_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_WALL_LAW_2D);
                            if ( strstr(BoundStr,"CT_WALL_NO_SLIP_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_WALL_NO_SLIP_2D);
                            if ( strstr(BoundStr,"CT_dRhodx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dRhodx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dUdx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dUdx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dVdx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dVdx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dTdx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dTdx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dYdx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dYdx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dRhody_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dRhody_NULL_2D);
                            if ( strstr(BoundStr,"CT_dUdy_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dUdy_NULL_2D);
                            if ( strstr(BoundStr,"CT_dVdy_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dVdy_NULL_2D);
                            if ( strstr(BoundStr,"CT_dTdy_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dTdy_NULL_2D);
                            if ( strstr(BoundStr,"CT_dYdy_NULL_2D_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dYdy_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Rhodx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Rhodx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Udx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Udx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Vdx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Vdx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Tdx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Tdx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Ydx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Ydx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Rhody2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Rhody2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Udy2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Udy2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Vdy2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Vdy2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Tdy2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Tdy2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Ydy2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Ydy2_NULL_2D);
                            if ( strstr(BoundStr,"CT_SOLID_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_SOLID_2D);
                            if ( strstr(BoundStr,"CT_BL_REFINEMENT_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_BL_REFINEMENT_2D);
                            if ( strstr(BoundStr,"CT_NONREFLECTED_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_NONREFLECTED_2D);

                            if ( strstr(BoundStr,"TCT_k_eps_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_k_eps_Model_2D);
                            else if (strstr(BoundStr,"TCT_Smagorinsky_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Smagorinsky_Model_2D);
                            else if (strstr(BoundStr,"TCT_Spalart_Allmaras_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Spalart_Allmaras_Model_2D);
                            else if (strstr(BoundStr,"TCT_Prandtl_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Prandtl_Model_2D);
                            else if (strstr(BoundStr,"TCT_Integral_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Integral_Model_2D);

                            if (isTurbulenceCond2D(TmpTurbulenceCT,TCT_k_eps_Model_2D) || 
                                isTurbulenceCond2D(TmpTurbulenceCT,TCT_Spalart_Allmaras_Model_2D)) {
                                if ( strstr(BoundStr,"TCT_k_CONST_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_k_CONST_2D);
                                if ( strstr(BoundStr,"TCT_eps_CONST_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_eps_CONST_2D);
                                if ( strstr(BoundStr,"TCT_dkdx_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_dkdx_NULL_2D);
                                if ( strstr(BoundStr,"TCT_depsdx_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_depsdx_NULL_2D);
                                if ( strstr(BoundStr,"TCT_dkdy_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_dkdy_NULL_2D);
                                if ( strstr(BoundStr,"TCT_depsdy_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_depsdy_NULL_2D);
                                if ( strstr(BoundStr,"TCT_d2kdx2_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_d2kdx2_NULL_2D);
                                if ( strstr(BoundStr,"TCT_d2epsdx2_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_d2epsdx2_NULL_2D);
                                if ( strstr(BoundStr,"TCT_d2kdy2_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_d2kdy2_NULL_2D);
                                if ( strstr(BoundStr,"TCT_d2epsdy2_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_d2epsdy2_NULL_2D);

                                if ( strstr(BoundStr,"TCT_eps_mud2kdx2_WALL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_eps_mud2kdx2_WALL_2D);
                                if ( strstr(BoundStr,"TCT_eps_mud2kdy2_WALL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_eps_mud2kdy2_WALL_2D);
                                if ( strstr(BoundStr,"TCT_eps_Cmk2kXn_WALL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_eps_Cmk2kXn_WALL_2D);
                            }

                            // Macro conditions
                            if ( strstr(BoundStr,"NT_AX_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_AX_2D);
                            else if ( strstr(BoundStr,"NT_AY_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_AY_2D);

                            if ( strstr(BoundStr,"NT_D0X_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_D0X_2D);
                            if ( strstr(BoundStr,"NT_D0Y_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_D0Y_2D);
                            if ( strstr(BoundStr,"NT_D2X_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_D2X_2D);
                            if ( strstr(BoundStr,"NT_D2Y_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_D2Y_2D);
                            if ( strstr(BoundStr,"NT_WALL_LAW_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_WALL_LAW_2D);
                            else if ( strstr(BoundStr,"NT_WNS_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_WNS_2D);
                            if ( strstr(BoundStr,"NT_FC_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_FC_2D);
                            if ( strstr(BoundStr,"NT_S_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_S_2D);
                            if ( strstr(BoundStr,"NT_FALSE_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_NODE_IS_SET_2D);
                            if ( strstr(BoundStr,"NT_FARFIELD_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_FARFIELD_2D);

                            if ( TmpCT==CT_NO_COND_2D ) {
                                *f_stream << "\n";
                                *f_stream << "Unknown condition type "<< BoundStr << " in "<< NameContour <<" \n" << flush;
                                f_stream->flush();
                                if ( GasSwapData!=0 ) {
                                    CloseSwapFile(GasSwapFileName,
                                                  GasSwapData,
                                                  FileSizeGas,
                                                  fd_g,
                                                  1);
                                    useSwapFile=0;
                                } else {
                                    delete J;
                                }
                                J=NULL;
                                Abort_OpenHyperFLOW2D();
                            }
                            // Check Flow2D at first ...
                            sprintf(NameBound,"%s.Flow2D",NameContour);
                            FlowIndex = Data->GetIntVal(NameBound); 
                            if ( FlowIndex < 1 ) {
                                *f_stream << "\n";
                                sprintf(NameBound,"%s.Flow",NameContour);
                                FlowIndex = Data->GetIntVal(NameBound);
                                if ( FlowIndex < 1 ) {
                                    *f_stream << "\n";
                                    *f_stream << "Bad Flow index [" << FlowIndex << "]\n"<< flush;
                                    f_stream->flush();
                                    Abort_OpenHyperFLOW2D();
                                }
                                if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
                                pTestFlow = FlowList->GetElement(FlowIndex-1);
                                sprintf(FlowStr,"Flow%i.CompIndex",FlowIndex);
                                isFlow2D=0;
                            } else if (FlowIndex <= (int)Flow2DList->GetNumElements()){
                                pTestFlow2D = Flow2DList->GetElement(FlowIndex-1);
                                sprintf(FlowStr,"Flow2D-%i.CompIndex",FlowIndex);
                                isFlow2D=1;
                            } else {
                                *f_stream << "\n";
                                *f_stream << "Bad Flow index [" << FlowIndex << "]\n"<< flush;
                                Abort_OpenHyperFLOW2D();
                            }

                            CompIndex = Data->GetIntVal(FlowStr);
                            if ( CompIndex==0 )      Y=Y_fuel;
                            else if ( CompIndex==1 ) Y=Y_ox;
                            else if ( CompIndex==2 ) Y=Y_cp;
                            else if ( CompIndex==3 ) Y=Y_air;
                            else if ( CompIndex==4 ) Y=Y_mix;

                            sprintf(NameBound,"%s.isReset",NameContour);
                            is_reset = Data->GetIntVal(NameBound);
                            if(!p_g) { 
                                is_reset      = 1; // If swapfile not exist, bounds reset always
                                isFirstStart  = 1;
                            }

                            sprintf(NameBound,"%s.MaterialID",NameContour);
                            BoundMaterialID = Data->GetIntVal(NameBound);
                            //}
                            *f_stream << "\nAdd object \"SingleBound" << i << "\"  ["<< s_x << ";"<< s_y <<"]" << flush;

                                if(!is_reset)
                                    pTestFlow = pTestFlow2D=NULL;

                                if ( !isFlow2D )
                                    SingleBound = new Bound2D(NameContour,J,s_x,s_y,e_x,e_y,TmpCT, pTestFlow,Y,TmpTurbulenceCT);
                                else
                                    SingleBound = new Bound2D(NameContour,J,s_x,s_y,e_x,e_y,TmpCT, pTestFlow2D,Y,TmpTurbulenceCT);

                                *f_stream << "-["<< e_x << ";"<< e_y <<"]" << flush;
                                
                                if(is_reset)
                                   *f_stream << "...Reset parameters\n" << flush;
                                else
                                   *f_stream << "\n" << flush;
                                
                                if(is_reset || !PreloadFlag )
                                   SingleBound->SetBound(BoundMaterialID);
                                
                                delete SingleBound;
                                f_stream->flush();
                            }
                /* end SingleBound loading */
                /* Set bounds */
                //if ( !PreloadFlag )
                   *f_stream << "\nNum contours: " <<  NumContour << endl;
                   for (int jc=0;jc<NumContour;jc++ ) {
                       sprintf(NameContour,"Contour%i",jc+1);
                        ContourTable = Data->GetTable(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        ix=max((int)(ContourTable->GetX(0)/dx),0);
                        iy=max((int)(ContourTable->GetY(0)/dy-1),0);
                        BC = new BoundContour2D(NameContour,J,ix,iy);

                        *f_stream << "Add object \""<< NameContour << "\"...\n" << flush;
                        f_stream->flush();

                        BoundMaterialID = 0;
                        sprintf(NameBound,"%s.MaterialID",NameContour);
                        BoundMaterialID = Data->GetIntVal(NameBound);

                        for (int i=1;i<(int)ContourTable->GetNumNodes()+1;i++ ) {
                            i_last = ContourTable->GetNumNodes()+1;
                            sprintf(NameBound,"%s.Bound%i.Cond",NameContour,i);
                            BoundStr = Data->GetStringVal(NameBound);
                            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                            sprintf(NameBound,"%s.Bound%i.TurbulenceModel",NameContour,i);
                            int TurbMod=Data->GetIntVal(NameBound);
                            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                            TmpCT           = CT_NO_COND_2D;
                            TmpTurbulenceCT = TCT_No_Turbulence_2D;

                            if ( TurbMod == 0 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_No_Turbulence_2D);
                            else if ( TurbMod == 1 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Integral_Model_2D);
                            else if ( TurbMod == 2 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Prandtl_Model_2D);
                            else if ( TurbMod == 3 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Spalart_Allmaras_Model_2D);
                            else if ( TurbMod == 4 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_k_eps_Model_2D);
                            else if ( TurbMod == 5 )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Smagorinsky_Model_2D);

                            // Simple conditions
                            if ( strstr(BoundStr,"CT_Rho_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_Rho_CONST_2D);
                            if ( strstr(BoundStr,"CT_U_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_U_CONST_2D);
                            if ( strstr(BoundStr,"CT_V_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_V_CONST_2D);
                            if ( strstr(BoundStr,"CT_T_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_T_CONST_2D);
                            if ( strstr(BoundStr,"CT_Y_CONST_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_Y_CONST_2D);
                            if ( strstr(BoundStr,"CT_dRhodx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dRhodx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dUdx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dUdx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dVdx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dVdx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dTdx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dTdx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dYdx_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dYdx_NULL_2D);
                            if ( strstr(BoundStr,"CT_dRhody_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dRhody_NULL_2D);
                            if ( strstr(BoundStr,"CT_dUdy_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dUdy_NULL_2D);
                            if ( strstr(BoundStr,"CT_dVdy_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dVdy_NULL_2D);
                            if ( strstr(BoundStr,"CT_dTdy_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dTdy_NULL_2D);
                            if ( strstr(BoundStr,"CT_dYdy_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_dYdy_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Rhodx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Rhodx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Udx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Udx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Vdx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Vdx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Tdx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Tdx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Ydx2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Ydx2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Rhody2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Rhody2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Udy2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Udy2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Vdy2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Vdy2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Tdy2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Tdy2_NULL_2D);
                            if ( strstr(BoundStr,"CT_d2Ydy2_NULL_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_d2Ydy2_NULL_2D);
                            if ( strstr(BoundStr,"CT_SOLID_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_SOLID_2D);
                            if ( strstr(BoundStr,"CT_BL_REFINEMENT_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_BL_REFINEMENT_2D);
                            if ( strstr(BoundStr,"CT_NONREFLECTED_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_NONREFLECTED_2D);


                            if ( strstr(BoundStr,"TCT_k_eps_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_k_eps_Model_2D);
                            else if (strstr(BoundStr,"TCT_Smagorinsky_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Smagorinsky_Model_2D);
                            else if ( strstr(BoundStr,"TCT_Spalart_Allmaras_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Spalart_Allmaras_Model_2D);
                            else if (strstr(BoundStr,"TCT_Prandtl_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Prandtl_Model_2D);
                            else if (strstr(BoundStr,"TCT_Integral_Model_2D") )
                                TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_Integral_Model_2D);

                            if (isTurbulenceCond2D(TmpTurbulenceCT,TCT_k_eps_Model_2D) ||
                                isTurbulenceCond2D(TmpTurbulenceCT,TCT_Spalart_Allmaras_Model_2D)) {
                                if ( strstr(BoundStr,"TCT_k_CONST_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_k_CONST_2D);
                                if ( strstr(BoundStr,"TCT_eps_CONST_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_eps_CONST_2D);
                                if ( strstr(BoundStr,"TCT_dkdx_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_dkdx_NULL_2D);
                                if ( strstr(BoundStr,"TCT_depsdx_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_depsdx_NULL_2D);
                                if ( strstr(BoundStr,"TCT_dkdy_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_dkdy_NULL_2D);
                                if ( strstr(BoundStr,"TCT_depsdy_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_depsdy_NULL_2D);
                                if ( strstr(BoundStr,"TCT_d2kdx2_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_d2kdx2_NULL_2D);
                                if ( strstr(BoundStr,"TCT_d2epsdx2_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_d2epsdx2_NULL_2D);
                                if ( strstr(BoundStr,"TCT_d2kdy2_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_d2kdy2_NULL_2D);
                                if ( strstr(BoundStr,"TCT_d2epsdy2_NULL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_d2epsdy2_NULL_2D);

                                if ( strstr(BoundStr,"TCT_eps_mud2kdx2_WALL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_eps_mud2kdx2_WALL_2D);
                                if ( strstr(BoundStr,"TCT_eps_mud2kdy2_WALL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_eps_mud2kdy2_WALL_2D);
                                if ( strstr(BoundStr,"TCT_eps_Cmk2kXn_WALL_2D") )
                                    TmpTurbulenceCT = (TurbulenceCondType2D)(TmpTurbulenceCT | TCT_eps_Cmk2kXn_WALL_2D);
                            }

                            // Macro conditions
                            if ( strstr(BoundStr,"NT_AX_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_AX_2D);
                            else if ( strstr(BoundStr,"NT_AY_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_AY_2D);

                            if ( strstr(BoundStr,"NT_D0X_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_D0X_2D);
                            if ( strstr(BoundStr,"NT_D0Y_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_D0Y_2D);
                            if ( strstr(BoundStr,"NT_D2X_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_D2X_2D);
                            if ( strstr(BoundStr,"NT_D2Y_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_D2Y_2D);
                            if ( strstr(BoundStr,"NT_WALL_LAW_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_WALL_LAW_2D);
                            else if ( strstr(BoundStr,"NT_WNS_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_WNS_2D);
                            if ( strstr(BoundStr,"NT_FC_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_FC_2D);
                            if ( strstr(BoundStr,"NT_S_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_S_2D);
                            if ( strstr(BoundStr,"NT_FALSE_2D") )
                                TmpCT = (CondType2D)(TmpCT | CT_NODE_IS_SET_2D);
                            if ( strstr(BoundStr,"NT_FARFIELD_2D") )
                                TmpCT = (CondType2D)(TmpCT | NT_FARFIELD_2D);


                            if ( TmpCT==CT_NO_COND_2D  &&  TmpTurbulenceCT == 0) {
                                *f_stream << "\n";
                                *f_stream << "Unknown condition type "<< BoundStr << " in "<< NameBound <<" \n" << flush;
                                f_stream->flush();
                                if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                                    CloseSwapFile(GasSwapFileName,
                                                  GasSwapData,
                                                  FileSizeGas,
                                                  fd_g,
                                                  1);
#endif // _REMOVE_SWAPFILE_
                                    useSwapFile=0;
                                } else {
                                    delete J;
                                }
                                J=NULL;
                                Abort_OpenHyperFLOW2D();
                            }
                            // Check Flow2D at first ...
                            sprintf(NameBound,"%s.Bound%i.Flow2D",NameContour,i);
                            FlowIndex = Data->GetIntVal(NameBound);
                            if ( FlowIndex < 1 ) {
                                *f_stream << "\n";
                                sprintf(NameBound,"%s.Flow",NameContour);
                                FlowIndex = Data->GetIntVal(NameBound);
                                if ( FlowIndex < 1 ) {
                                    *f_stream << "\n";
                                    *f_stream << "Bad Flow index [" << FlowIndex << "]\n"<< flush;
                                    f_stream->flush();
                                    Abort_OpenHyperFLOW2D();
                                }
                                if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
                                pTestFlow = FlowList->GetElement(FlowIndex-1);
                                sprintf(FlowStr,"Flow%i.CompIndex",FlowIndex);
                                isFlow2D=0;
                            } else if (FlowIndex <= (int)Flow2DList->GetNumElements()){
                                pTestFlow2D = Flow2DList->GetElement(FlowIndex-1);
                                sprintf(FlowStr,"Flow2D-%i.CompIndex",FlowIndex);
                                isFlow2D=1;
                            } else {
                                *f_stream << "\n";
                                *f_stream << "Bad Flow index [" << FlowIndex << "]\n"<< flush;
                                Abort_OpenHyperFLOW2D();
                            }

                            CompIndex = Data->GetIntVal(FlowStr);
                            if ( CompIndex==0 )      Y=Y_fuel;
                            else if ( CompIndex==1 ) Y=Y_ox;
                            else if ( CompIndex==2 ) Y=Y_cp;
                            else if ( CompIndex==3 ) Y=Y_air;
                            else if ( CompIndex==4 ) Y=Y_mix;

                            sprintf(NameBound,"%s.Bound%i.isReset",NameContour,i);
                            is_reset = Data->GetIntVal(NameBound);
                            if(!p_g) is_reset = 1; // If swapfile not exist, bounds reset always
                            *f_stream << "Add object \"Bound" << i << "\"  ["<< BC->GetCurrentX() << ";"<< BC->GetCurrentY() <<"]" << flush;
                            if ( i < (int)ContourTable->GetNumNodes() ) {
                                ix=max((int)(ContourTable->GetX(i)/dx),0);
                                iy=max((int)(ContourTable->GetY(i)/dy-1),0);

                                if(!is_reset)
                                    pTestFlow = pTestFlow2D=NULL;
                                sprintf(NameBound,"%s.Bound%i",NameContour,i);
                                if ( !isFlow2D )
                                    BC->AddBound2D(NameBound,ix,iy,TmpCT,pTestFlow,NULL,Y,TmpTurbulenceCT);
                                else
                                    BC->AddBound2D(NameBound,ix,iy,TmpCT,NULL,pTestFlow2D,Y,TmpTurbulenceCT);

                                *f_stream << "-["<< BC->GetCurrentX() << ";"<< BC->GetCurrentY() <<"]" << flush;

                                if(is_reset)
                                   *f_stream << "...Reset parameters" << flush;
                                *f_stream << "\n";
                                f_stream->flush();
                            }
                        }

                        
                        //pTestFlow = pTestFlow2D = NULL;

                        sprintf(NameBound,"%s.Bound%i",NameContour,i_last);

                        if ( !isFlow2D )
                            BC->CloseContour2D(NameBound,TmpCT,pTestFlow,NULL,Y,TmpTurbulenceCT);
                        else
                            BC->CloseContour2D(NameBound,TmpCT,NULL,pTestFlow2D,Y,TmpTurbulenceCT);
                        
                        *f_stream << "-["<< BC->GetCurrentX() << ";"<< BC->GetCurrentY() <<"]" << flush;
                        
                        if(is_reset)
                           *f_stream << "...Reset parameters" << flush;
                        *f_stream << "\nEnd bounds..." << flush;
                        f_stream->flush();

                        if ( !BC->IsContourClosed() ) {
                            *f_stream << "\n";
                            *f_stream << "Contour is not looped.\n" << flush;
                            f_stream->flush();
                            Abort_OpenHyperFLOW2D();
                        }
                        *f_stream << "OK\nSet bounds for \""<< NameContour << "\"..." << flush;
                        f_stream->flush();
                        
                        if(is_reset || !PreloadFlag)
                           BC->SetBounds(BoundMaterialID);
                        
                        *f_stream << "OK\n" << flush;
                        f_stream->flush();
                        delete BC;
                   }
#ifdef _DEBUG_0
            }__except(int num_bound) {
                *f_stream << "\nERROR:";
                *f_stream << "Set Bound error (bound No. "<< num_bound+1 << ") in \""<< NameContour <<"\"\n" << flush;
                f_stream->flush();
                if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                    CloseSwapFile(GasSwapFileName,
                                  GasSwapData,
                                  FileSizeGas,
                                  fd_g,
                                  1);
#endif // _REMOVE_SWAPFILE_
                    useSwapFile = 0;
                } else {
                    delete J;
                }
                J=NULL;
                Abort_OpenHyperFLOW2D();
            }__except(void*) {
                *f_stream << " \nERROR: ";
                *f_stream << "Unknown Bound error in \""<< NameContour <<"\"\n" << flush;
                f_stream->flush();
                if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                    CloseSwapFile(GasSwapFileName,
                                  GasSwapData,
                                  FileSizeGas,
                                  fd_g,
                                  1);
#endif // _REMOVE_SWAPFILE_
                    useSwapFile = 0;
                } else {
                    delete J;
                }
                J=NULL;
                Abort_OpenHyperFLOW2D();
            }
            __end_except;
#endif // _DEBUG_0

            dt = 1;

            for (int i = 0;i<(int)Flow2DList->GetNumElements();i++ ) {
                FP CFL_min  = min(CFL0,CFL_Scenario->GetVal(iter+last_iter));
                dt = min(dt,CFL_min*min(dx/(Flow2DList->GetElement(i)->Asound()+Flow2DList->GetElement(i)->Wg()),
                                        dy/(Flow2DList->GetElement(i)->Asound()+Flow2DList->GetElement(i)->Wg())));
            }
            
            if ( !PreloadFlag ) {
                CutFile(OutFileName);  
                *f_stream << "Init computation area..." << flush;
                f_stream->flush();
#ifdef _DEBUG_0
           ___try {
#endif  // _DEBUG_0
                    for (int j=0;j<(int)MaxY;j++ )
                        for (int i=0;i<(int)MaxX;i++ ) {
                          i_err = i;
                          j_err = j;
#ifndef _UNIFORM_MESH_
                          J->GetValue(i,j).dx  = dx;
                          J->GetValue(i,j).dy  = dy;
#endif //_UNIFORM_MESH_

                          //if ( FlowNode2D<FP,NUM_COMPONENTS>::FT == FT_AXISYMMETRIC )
                          //     J->GetValue(i,j).r     = (j+0.5)*dy;
                          J->GetValue(i,j).x     = (i+0.5)*dx;
                          J->GetValue(i,j).y     = (j+0.5)*dy;
                          J->GetValue(i,j).ix    = i;
                          J->GetValue(i,j).iy    = j;
                          J->GetValue(i,j).Tf    = chemical_reactions.Tf;
                          J->GetValue(i,j).BGX   = 1.;
                          J->GetValue(i,j).BGY   = 1.;
                          J->GetValue(i,j).NGX   = 0;
                          J->GetValue(i,j).NGY   = 0;

                          for ( k=0;k<(int)FlowNode2D<FP,NUM_COMPONENTS>::NumEq;k++ )
                              J->GetValue(i,j).Src[k]= J->GetValue(i,j).SrcAdd[k] = 0;
                        }

#ifdef _DEBUG_0
                }__except(SysException e) {
                    //int isDel=0;
                    if ( e == SIGINT ) {
                        *f_stream << "\n";
                        *f_stream << "Interrupted by user\n" <<  flush;
                    } else {
                        *f_stream << "\n";
                        *f_stream << "Error init computation area ("<<i_err<<","<<j_err<<")..." << flush;
                        *f_stream << "\n";
                        *f_stream << "System error " << e << " \n" << flush;
                    }
                    f_stream->flush();
                    if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                        if ( e != SIGINT ) isDel = 1;
#endif //_REMOVE_SWAPFILE_
#ifdef _REMOVE_SWAPFILE_
      CloseSwapFile(GasSwapFileName,
                    GasSwapData,
                    FileSizeGas,
                    fd_g,
                    isDel);
#endif // _REMOVE_SWAPFILE_
                    } else {
                      delete J;
                    }
                      J=NULL;
                    Abort_OpenHyperFLOW2D();
                } __except( ComputationalMatrix2D*  m) {
                    if ( m->GetMatrixState()==MXS_ERR_OUT_OF_INDEX ) {
                        *f_stream << "\n";
                        *f_stream << "Error init computation area..("<<i_err<<","<<j_err<<")..." << flush;
                        *f_stream << "\n";
                        *f_stream << "Matrix out of index error\n" << flush;
                        f_stream->flush();
                        if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                            CloseSwapFile(GasSwapFileName,
                                          GasSwapData,
                                          FileSizeGas,
                                          fd_g,
                                          1);
#endif // _REMOVE_SWAPFILE_
                            useSwapFile = 0;
                        } else {
                            delete J;
                        }
                        J=NULL;
                        Abort_OpenHyperFLOW2D();
                    }
                } __except(void*) {
                    *f_stream << "\n";
                    *f_stream << "Error init computation area..("<<i_err<<","<<j_err<<")..." << flush;
                    *f_stream << "Unknown error\n" << flush;
                    f_stream->flush();
                    if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                        CloseSwapFile(GasSwapFileName,
                                      GasSwapData,
                                      FileSizeGas,
                                      fd_g,
                                      1);
#endif // _REMOVE_SWAPFILE_
                        useSwapFile = 0;
                    } else {
                        delete J;
                    }
                    J=NULL;
                    Abort_OpenHyperFLOW2D();
                }
                __end_except;
#endif  // _DEBUG_0

                *f_stream << "OK\n" << flush;
                f_stream->flush();
                /* End set bounds */
            }
            /* -- if use swapfile */

            //Cx,Cy calc
            is_Cx_calc = Data->GetIntVal((char*)"is_Cx_calc");
            if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();

            if (is_Cx_calc) {
             x0_body=Data->GetFloatVal((char*)"x_body");
             if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();
             y0_body=Data->GetFloatVal((char*)"y_body");
             if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();
             dx_body=Data->GetFloatVal((char*)"dx_body");
             if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();
             dy_body=Data->GetFloatVal((char*)"dy_body");
             if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();
             Cx_Flow_index = Data->GetIntVal((char*)"Cx_Flow_Index");
            }
            //Set bound Primitives
            // Rects
                FP        Xstart,Ystart,X_0,Y_0;
                unsigned int  numRects=Data->GetIntVal((char*)"NumRects");
                if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();
                //unsigned int ix0,iy0;
                SolidBoundRect2D* SBR;
                GlobalTime=Data->GetFloatVal((char*)"InitTime");
                if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();

                 if(!PreloadFlag && numRects) {
                   for(j=0;j<numRects;j++) {

                       TurbulenceCondType2D TM;

                       sprintf(NameContour,"Rect%i",j+1);
                       *f_stream << "Add object \""<< NameContour << "\"..." << flush;

                       sprintf(NameContour,"Rect%i.Xstart",j+1);
                       Xstart=Data->GetFloatVal(NameContour);
                       if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();

                       sprintf(NameContour,"Rect%i.Ystart",j+1);
                       Ystart=Data->GetFloatVal(NameContour);
                       if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();

                       sprintf(NameContour,"Rect%i.DX",j+1);
                       X_0=Data->GetFloatVal(NameContour);
                       if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();

                       sprintf(NameContour,"Rect%i.DY",j+1);
                       Y_0=Data->GetFloatVal(NameContour);
                       if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();

                       sprintf(NameContour,"Rect%i.Flow2D",j+1);
                       FlowIndex=Data->GetIntVal(NameContour);
                       if(Data->GetDataError()==-1) Abort_OpenHyperFLOW2D();

                       sprintf(NameContour,"Rect%i.TurbulenceModel",j+1);
                       int TurbMod=Data->GetIntVal(NameContour);
                       if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                       chord = max(chord,X_0);

                       TM = TCT_No_Turbulence_2D;
                       if ( TurbMod == 0 )
                           TM = (TurbulenceCondType2D)(TM | TCT_No_Turbulence_2D);
                       else if ( TurbMod == 1 )
                           TM = (TurbulenceCondType2D)(TM | TCT_Integral_Model_2D);
                       else if ( TurbMod == 2 )
                           TM = (TurbulenceCondType2D)(TM | TCT_Prandtl_Model_2D);
                       else if ( TurbMod == 3 )
                           TM = (TurbulenceCondType2D)(TM | TCT_Spalart_Allmaras_Model_2D);
                       else if ( TurbMod == 4 )
                           TM = (TurbulenceCondType2D)(TM | TCT_k_eps_Model_2D);
                       else if ( TurbMod == 5 )
                           TM = (TurbulenceCondType2D)(TM | TCT_Smagorinsky_Model_2D);
                       if(FlowIndex < 1 || FlowIndex > (int)Flow2DList->GetNumElements())
                         {
                           *f_stream << "\nBad Flow index [" << FlowIndex << "]\n"<< flush;
                           f_stream->flush();
                           Abort_OpenHyperFLOW2D();
                         }

                       pTestFlow2D = Flow2DList->GetElement(FlowIndex-1);
                       snprintf(FlowStr,256,"Flow2D-%i.CompIndex",FlowIndex);

                       CompIndex = Data->GetIntVal(FlowStr);
                       if(CompIndex==0)      Y=Y_fuel;
                       else if(CompIndex==1) Y=Y_ox;
                       else if(CompIndex==2) Y=Y_cp;
                       else if(CompIndex==3) Y=Y_air;
                       else if(CompIndex==4) Y=Y_mix;
                       else {
                           *f_stream << "\nBad component index [" << CompIndex << "]\n"<< flush;
                           f_stream->flush();
                           Abort_OpenHyperFLOW2D();
                       }
                       sprintf(NameContour,"Rect%i",j+1);
                       SBR = new SolidBoundRect2D(NameContour,J,Xstart,Ystart,X_0,Y_0,dx,dy,(CondType2D)NT_WNS_2D,pTestFlow2D,Y,TM);
                       *f_stream << "OK\n" << flush;
                       delete SBR;
                     }
                  }
            // Solid Bound Circles
            unsigned int numCircles=Data->GetIntVal((char*)"NumCircles");
            TurbulenceCondType2D TM;
            BoundCircle2D* SBC;
               if ( !PreloadFlag && numCircles ) {
                    for ( j=0;j<numCircles;j++ ) {
                        sprintf(NameContour,"Circle%i",j+1);
                        *f_stream << "Add object \""<< NameContour << "\"..." << flush;

                        sprintf(NameContour,"Circle%i.Xstart",j+1);
                        Xstart=Data->GetFloatVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        sprintf(NameContour,"Circle%i.Ystart",j+1);
                        Ystart=Data->GetFloatVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        sprintf(NameContour,"Circle%i.X0",j+1);
                        X_0=Data->GetFloatVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        sprintf(NameContour,"Circle%i.Y0",j+1);
                        Y_0=Data->GetFloatVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
                        
                        sprintf(NameContour,"Circle%i.MaterialID",j+1);
                        int MaterialID=Data->GetIntVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        sprintf(NameContour,"Circle%i.TurbulenceModel",j+1);
                        int TurbMod=Data->GetIntVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        chord = max(chord,sqrt((X_0-Xstart)*(X_0-Xstart) + (Y_0-Ystart*(Y_0-Ystart) )+1.e-60));

                        TM = TCT_No_Turbulence_2D;
                        if ( TurbMod == 0 )
                            TM = (TurbulenceCondType2D)(TM | TCT_No_Turbulence_2D);
                        else if ( TurbMod == 1 )
                            TM = (TurbulenceCondType2D)(TM | TCT_Integral_Model_2D);
                        else if ( TurbMod == 2 )
                            TM = (TurbulenceCondType2D)(TM | TCT_Prandtl_Model_2D);
                        else if ( TurbMod == 3 )
                            TM = (TurbulenceCondType2D)(TM | TCT_Spalart_Allmaras_Model_2D);
                        else if ( TurbMod == 4 )
                            TM = (TurbulenceCondType2D)(TM | TCT_k_eps_Model_2D);
                        else if ( TurbMod == 5 )
                            TM = (TurbulenceCondType2D)(TM | TCT_Smagorinsky_Model_2D);

                        sprintf(NameContour,"Circle%i.Flow2D",j+1);
                        FlowIndex=Data->GetIntVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        if ( FlowIndex < 1 || FlowIndex > (int)Flow2DList->GetNumElements() ) {
                            *f_stream << "\n";
                            *f_stream << "Bad Flow index [" << FlowIndex << "]\n"<< flush;
                            f_stream->flush();
                            Abort_OpenHyperFLOW2D();
                        }

                        pTestFlow2D = Flow2DList->GetElement(FlowIndex-1);
                        snprintf(FlowStr,256,"Flow2D-%i.CompIndex",FlowIndex);

                        CompIndex = Data->GetIntVal(FlowStr);
                        if ( CompIndex==0 )      Y=Y_fuel;
                        else if ( CompIndex==1 ) Y=Y_ox;
                        else if ( CompIndex==2 ) Y=Y_cp;
                        else if ( CompIndex==3 ) Y=Y_air;
                        else if(CompIndex==4)    Y=Y_mix;
                        else {
                            *f_stream << "\n";
                            *f_stream << "Bad component index [" << CompIndex << "]\n"<< flush;
                            f_stream->flush();
                            Abort_OpenHyperFLOW2D();
                        }
                        sprintf(NameContour,"Circle%i",j+1);
                        if(MaterialID) { // Solid
                            SBC = new BoundCircle2D(NameContour,J,Xstart,Ystart,X_0,Y_0,dx,dy,(CondType2D)NT_WNS_2D,MaterialID,pTestFlow2D,Y,TM
#ifdef _DEBUG_1
                                                    ,f_stream
#endif //_DEBUG_1 
                                                    );
                        } else {         // GAS
                            SBC = new BoundCircle2D(NameContour,J,Xstart,Ystart,X_0,Y_0,dx,dy,(CondType2D)CT_NODE_IS_SET_2D,MaterialID,pTestFlow2D,Y,TM
#ifdef _DEBUG_1
                                                    ,f_stream
#endif //_DEBUG_1 
                                                    );
                        }
                        *f_stream << "OK\n" << flush;
                        delete SBC;
                    }
                }
// Solid Bound Airfoils
            unsigned int  numAirfoils=Data->GetIntVal((char*)"NumAirfoils");
            FP            mm,pp,thick,scale,attack_angle;
            int           Airfoil_Type;
            InputData*    AirfoilInputData=NULL;
            
            mm = pp = thick = 0;
            
            SolidBoundAirfoil2D* SBA;
                if (!PreloadFlag && numAirfoils ) {
                    for ( j=0;j<numAirfoils;j++ ) {
                        sprintf(NameContour,"Airfoil%i",j+1);    
                        *f_stream << "Add object \""<< NameContour << "\"..." << flush;

                        sprintf(NameContour,"Airfoil%i.Xstart",j+1);
                        Xstart=Data->GetFloatVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        sprintf(NameContour,"Airfoil%i.Ystart",j+1);
                        Ystart=Data->GetFloatVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();


                        sprintf(NameContour,"Airfoil%i.Type",j+1);
                        Airfoil_Type=Data->GetFloatVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        if (Airfoil_Type == 0) { // Embedded NACA XXXX Airfoil
                            sprintf(NameContour,"Airfoil%i.pp",j+1);
                            pp=Data->GetFloatVal(NameContour);
                            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                            sprintf(NameContour,"Airfoil%i.mm",j+1);
                            mm=Data->GetFloatVal(NameContour);
                            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                            sprintf(NameContour,"Airfoil%i.thick",j+1);
                            thick=Data->GetFloatVal(NameContour);
                            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
                        } else { // External Airfoil
                           char* AirfoilInputDataFileName;
                           sprintf(NameContour,"Airfoil%i.InputData",j+1);
                           AirfoilInputDataFileName=Data->GetStringVal(NameContour);
                           if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
                           AirfoilInputData = new InputData(AirfoilInputDataFileName,DS_FILE,f_stream);
                        }

                        sprintf(NameContour,"Airfoil%i.scale",j+1);
                        scale=Data->GetFloatVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();
                        
                        chord = max(chord,scale);

                        sprintf(NameContour,"Airfoil%i.attack_angle",j+1);    
                        attack_angle=Data->GetFloatVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        sprintf(NameContour,"Airfoil%i.Flow2D",j+1);    
                        FlowIndex=Data->GetIntVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        sprintf(NameContour,"Airfoil%i.TurbulenceModel",j+1);    
                        int TurbMod=Data->GetIntVal(NameContour);
                        if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                        TM = TCT_No_Turbulence_2D;
                        if ( TurbMod == 0 )
                            TM = (TurbulenceCondType2D)(TM | TCT_No_Turbulence_2D);
                        else if ( TurbMod == 1 )
                            TM = (TurbulenceCondType2D)(TM | TCT_Integral_Model_2D);
                        else if ( TurbMod == 2 )
                            TM = (TurbulenceCondType2D)(TM | TCT_Prandtl_Model_2D);
                        else if ( TurbMod == 3 )
                            TM = (TurbulenceCondType2D)(TM | TCT_Spalart_Allmaras_Model_2D);
                        else if ( TurbMod == 4 )
                            TM = (TurbulenceCondType2D)(TM | TCT_k_eps_Model_2D);
                        else if ( TurbMod == 5 )
                            TM = (TurbulenceCondType2D)(TM | TCT_Smagorinsky_Model_2D);

                        if ( FlowIndex < 1 || FlowIndex > (int)Flow2DList->GetNumElements() ) {
                            *f_stream << "\n";
                            *f_stream << "Bad Flow index [" << FlowIndex << "]\n"<< flush;
                            f_stream->flush();
                            Abort_OpenHyperFLOW2D();
                        }

                        pTestFlow2D = Flow2DList->GetElement(FlowIndex-1);
                        snprintf(FlowStr,256,"Flow2D-%i.CompIndex",FlowIndex);

                        CompIndex = Data->GetIntVal(FlowStr);
                        if ( CompIndex==0 )      Y=Y_fuel;
                        else if ( CompIndex==1 ) Y=Y_ox;
                        else if ( CompIndex==2 ) Y=Y_cp;
                        else if ( CompIndex==3 ) Y=Y_air;
                        else if ( CompIndex==4 ) Y=Y_mix;
                        else {
                            *f_stream << "\n";
                            *f_stream << "Bad component index [" << CompIndex << "]\n"<< flush;
                            f_stream->flush();
                            Abort_OpenHyperFLOW2D();
                        }

                        *f_stream  << "Airfoil Re = "  << Re_Airfoil(chord,Flow2DList->GetElement(Cx_Flow_index-1)) << endl;

                        sprintf(NameContour,"Airfoil%i",j+1);
                        if (Airfoil_Type == 0) { // Embedded NACA XXXX Airfoil
                           SBA = new SolidBoundAirfoil2D(NameContour,J,Xstart,Ystart,mm,pp,thick,dx,dy,(CondType2D)NT_WNS_2D,pTestFlow2D,Y,TM,scale,attack_angle,f_stream);
                        } else {                // External Airfoil
                           SBA = new SolidBoundAirfoil2D(NameContour,J,Xstart,Ystart, AirfoilInputData,dx,dy,(CondType2D)NT_WNS_2D,pTestFlow2D,Y,TM,scale,attack_angle,f_stream);
                        }
                        *f_stream << "OK\n" << flush;
                        delete SBA;
                    }
                }
                //  Areas
            if ( !PreloadFlag )
#ifdef _DEBUG_0
           ___try {
#endif  // _DEBUG_0
                    static Area2D*  TmpArea=NULL; 
                    static char     AreaName[256];
                    static int      AreaType;
                    static Table*   AreaPoint=NULL;
                    static int      AreaMaterialID;

                    for (int i=0;i<(int)NumArea;i++ ) {
                        snprintf(AreaName,256,"Area%i",i+1);

                        TmpArea = new Area2D(AreaName,J);
                        pTestFlow = pTestFlow2D = NULL;

                        *f_stream << "Add object \""<< AreaName << "\":" << flush;
                        f_stream->flush();
                        AreaPoint = Data->GetTable(AreaName);
                        if ( Data->GetDataError()==-1 ) {
                            Abort_OpenHyperFLOW2D();
                        }
                        snprintf(AreaName,256,"Area%i.Type",i+1);
                        AreaType = Data->GetIntVal(AreaName);
                        if ( Data->GetDataError()==-1 ) {
                            Abort_OpenHyperFLOW2D();
                        }
                        snprintf(AreaName,256,"initial Area point (%d,%d)...",(unsigned int)AreaPoint->GetX(0),
                                                                              (unsigned int)AreaPoint->GetY(0));
                        *f_stream << AreaName; 
                        
                        snprintf(AreaName,256,"Area%i.MaterialID",i+1);
                        AreaMaterialID = Data->GetIntVal(AreaName);
                        
                        if ( AreaType==0 ) {
                            if ( Data->GetDataError()==-1 ) {
                                Abort_OpenHyperFLOW2D();
                            }
                            TmpArea->FillArea2D((unsigned int)AreaPoint->GetX(0),
                                                (unsigned int)AreaPoint->GetY(0),
                                                CT_SOLID_2D,
                                                TCT_No_Turbulence_2D,
                                                AreaMaterialID);
                        } else if ( AreaType==1 ) {
                            //AreaMaterialID = GAS_ID;
                            snprintf(AreaName,256,"Area%i.Flow2D",i+1);
                            FlowIndex = Data->GetIntVal(AreaName);

                            if ( FlowIndex < 1 || FlowIndex > (int)Flow2DList->GetNumElements() ) {
                                *f_stream << "\n";
                                *f_stream << "Bad Flow index [" << FlowIndex << "] \n"<< flush;
                                f_stream->flush();
                                Abort_OpenHyperFLOW2D();
                            }

                            if ( Data->GetDataError()==-1 ) {
                                snprintf(AreaName,256,"Area%i.Flow",i+1);
                                FlowIndex = Data->GetIntVal(AreaName);
                                
                                if ( FlowIndex < 1 || FlowIndex > (int)Flow2DList->GetNumElements() ) {
                                    *f_stream << "\n";
                                    *f_stream << "Bad Flow index [" << FlowIndex << "]\n"<< flush;
                                    f_stream->flush();
                                    Abort_OpenHyperFLOW2D();
                                }
                                if ( Data->GetDataError()==-1 ) {
                                    Abort_OpenHyperFLOW2D();
                                }
                                pTestFlow = FlowList->GetElement(FlowIndex-1);
                            } else {
                                pTestFlow2D = Flow2DList->GetElement(FlowIndex-1);
                            }

                            if ( pTestFlow )
                                snprintf(FlowStr,256,"Flow%i.CompIndex",FlowIndex);
                            else if ( pTestFlow2D )
                                snprintf(FlowStr,256,"Flow2D-%i.CompIndex",FlowIndex);

                            CompIndex = Data->GetIntVal(FlowStr);
                            if ( Data->GetDataError()==-1 ) {
                                Abort_OpenHyperFLOW2D();
                            }

                            if ( CompIndex==0 )      Y=Y_fuel;
                            else if ( CompIndex==1 ) Y=Y_ox;
                            else if ( CompIndex==2 ) Y=Y_cp;
                            else if ( CompIndex==3 ) Y=Y_air;
                            else if ( CompIndex==4 ) Y=Y_mix;
                            else {
                                *f_stream << "Bad component index [" << CompIndex << "]\n" << flush;
                                Abort_OpenHyperFLOW2D();
                            }

                            sprintf(AreaName,"Area%i.TurbulenceModel",i+1);    
                            int TurbMod=Data->GetIntVal(AreaName);
                            if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

                            TM = TCT_No_Turbulence_2D;
                            if ( TurbMod == 0 )
                                TM = (TurbulenceCondType2D)(TM | TCT_No_Turbulence_2D);
                            else if ( TurbMod == 1 )
                                TM = (TurbulenceCondType2D)(TM | TCT_Integral_Model_2D);
                            else if ( TurbMod == 2 )
                                TM = (TurbulenceCondType2D)(TM | TCT_Prandtl_Model_2D);
                            else if ( TurbMod == 3 )
                                TM = (TurbulenceCondType2D)(TM | TCT_Spalart_Allmaras_Model_2D);
                            else if ( TurbMod == 4 )
                                TM = (TurbulenceCondType2D)(TM | TCT_k_eps_Model_2D);
                            else if ( TurbMod == 5 )
                                TM = (TurbulenceCondType2D)(TM | TCT_Smagorinsky_Model_2D);

                            if ( pTestFlow )
                                TmpArea->FillArea2D((unsigned int)AreaPoint->GetX(0),
                                                    (unsigned int)AreaPoint->GetY(0),
                                                    CT_NO_COND_2D,pTestFlow,Y,TM,AreaMaterialID);
                            else if ( pTestFlow2D )
                                TmpArea->FillArea2D((unsigned int)AreaPoint->GetX(0),
                                                    (unsigned int)AreaPoint->GetY(0),
                                                    CT_NO_COND_2D,pTestFlow2D,Y,TM,AreaMaterialID);
                        } else {
                            *f_stream << "\n";
                            *f_stream << "Bad Area type index \""<< AreaType <<"\" use in \"Area"<< i+1 <<"\"\n" << flush;
                            isRun=0;
                            f_stream->flush();
                            Abort_OpenHyperFLOW2D();
                        }
                        delete TmpArea;
                        *f_stream << "OK\n";
                        f_stream->flush();
                        TmpArea = NULL;
                        AreaPoint=NULL;
                    }
#ifdef _DEBUG_0
                }__except(Area2D* errAreaPtr) {
                *f_stream << "\n";
                *f_stream << "Error init computation area...\n";
                if ( errAreaPtr->GetAreaState() == AS_ERR_OUT_OF_RANGE ) {
                    *f_stream << "\n";
                    *f_stream << "Init Area point ["<< errAreaPtr->GetStartX()  <<"," << errAreaPtr->GetStartY() <<"] out of range.\n" << flush;
                } else if ( errAreaPtr->GetAreaState() == AS_ERR_INIT_POINT ) {
                    *f_stream << "\n";
                    *f_stream << "Init Area point ["<< errAreaPtr->GetStartX()  <<"," << errAreaPtr->GetStartY() <<"] already in initialized node.\n" << flush;
//-- debug ----
//                     DataSnapshot(ErrFileName);
//-- debug ----
                } else {
                    *f_stream << "\n";
                    *f_stream << "Unknown Init Area error.\n" << flush;
                }
                if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                    CloseSwapFile(GasSwapFileName,
                                  GasSwapData,
                                  FileSizeGas,
                                  fd_g,
                                  1);
#endif // _REMOVE_SWAPFILE_
                    useSwapFile = 0;
                } else {
                    delete J;
                } 
                J = NULL;
                f_stream->flush();
                Abort_OpenHyperFLOW2D();
            }__except( ComputationalMatrix2D*  m) {
                if ( m->GetMatrixState()==MXS_ERR_OUT_OF_INDEX ) {
                    *f_stream << "\n";
                    *f_stream << "Error init computation area..." << flush;
                    *f_stream << "\n";
                    *f_stream << "Matrix out of index error\n" << flush;
                    f_stream->flush();
                    isRun=0;
                    if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                        CloseSwapFile(GasSwapFileName,
                                      GasSwapData,
                                      FileSizeGas,
                                      fd_g,
                                      1);
#endif // _REMOVE_SWAPFILE_
                        useSwapFile = 0;
                    } else {
                        delete J;
                    }
                    J=NULL;
                    Abort_OpenHyperFLOW2D();
                }
            }__except(void*) {
                *f_stream << "\n";
                *f_stream << "Error init computation area..." << flush;
                *f_stream << "Unknown error\n" << flush;
                f_stream->flush();
                isRun=0;
                if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                    CloseSwapFile(GasSwapFileName,
                                  GasSwapData,
                                  FileSizeGas,
                                  fd_g,
                                  1);
#endif // _REMOVE_SWAPFILE_
                    useSwapFile = 0;  
                } else {
                    delete J;
                }
                J=NULL;
                Abort_OpenHyperFLOW2D();
            }
            __end_except;
#endif  // _DEBUG_0


            if ( !PreloadFlag )
#ifdef _DEBUG_0
              ___try {
#endif  // _DEBUG_0

                    *f_stream << "\nFirst initialization computation area...";
                    for (int i=0;i<(int)MaxX;i++ )
                        for (int j=0;j<(int)MaxY;j++ ) {

                                i_err = i;
                                j_err = j;

                                J->GetValue(i,j).idXl  = 1;
                                J->GetValue(i,j).idXr  = 1;
                                J->GetValue(i,j).idYu  = 1;
                                J->GetValue(i,j).idYd  = 1;
                                J->GetValue(i,j).l_min = min(dx*MaxX,dy*MaxY);
                                
                                for (int k=0;k<(int)FlowNode2D<FP,NUM_COMPONENTS>::NumEq;k++ )
                                    J->GetValue(i,j).beta[k]  = beta0;
                                
                                if ( j==0 || J->GetValue(i,j-1).isCond2D(CT_SOLID_2D) ) {           // is down node present ? (0 or 1)
                                    J->GetValue(i,j).idYd=0;
                                    if(isHighOrder) {
                                    }
                                }
                                if ( j==(int)MaxY-1 || J->GetValue(i,j+1).isCond2D(CT_SOLID_2D) ) { // is up node present ? (0 or 1)
                                    J->GetValue(i,j).idYu=0;
                                    if(isHighOrder) {
                                    }
                                }
                                if ( i==0 || J->GetValue(i-1,j).isCond2D(CT_SOLID_2D) ) {           // is left node present ? (0 or 1)
                                    J->GetValue(i,j).idXl=0;
                                    if(isHighOrder) {
                                    }
                                }
                                if ( i==(int)MaxX-1 ||J->GetValue(i+1,j).isCond2D(CT_SOLID_2D) ) {  // is right node present ? (0 or 1)
                                    J->GetValue(i,j).idXr=0;
                                    if(isHighOrder) {
                                    }
                                }

                                if (J->GetValue(i,j).isCond2D(CT_WALL_NO_SLIP_2D) || J->GetValue(i,j).isCond2D(CT_WALL_LAW_2D)) {
                                    J->GetValue(i,j).NGX   = J->GetValue(i,j).idXl - J->GetValue(i,j).idXr + J->GetValue(i,j).idXl*J->GetValue(i,j).idXr;
                                    J->GetValue(i,j).NGY   = J->GetValue(i,j).idYd - J->GetValue(i,j).idYu + J->GetValue(i,j).idYd*J->GetValue(i,j).idYu;
                                }

                                if (!isIgnoreUnsetNodes &&  !J->GetValue(i,j).isCond2D(CT_NODE_IS_SET_2D) ) {
                                    *f_stream << "\n";
                                    *f_stream << "Node ("<< i_err << "," << j_err <<") has not CT_NODE_IS_SET flag." << flush;
                                    *f_stream << "\n";
                                    *f_stream << "Possible some \"Area\" objects not defined.\n" << flush;
                                    f_stream->flush();
//-- debug ----
//                                    DataSnapshot(ErrFileName);
//-- debug ----
                                    Abort_OpenHyperFLOW2D();
                                }
                                
                                if (J->GetValue(i,j).isCond2D(CT_SOLID_2D) ) {
                                    J->GetValue(i,j).Tg = Ts0;
                                }   else {
                                    J->GetValue(i,j).FillNode2D(0,1);
                                }
                                
                                if(J->GetValue(i,j).p == 0.) {
                                   J->GetValue(i,j).Tg = Ts0;
                                }
                            }
                 *f_stream << "OK" << endl;
#ifdef _DEBUG_0
                }__except( ComputationalMatrix2D*  m) {
                *f_stream << "\n";
                *f_stream << "Error set computation area state ("<< i_err << "," << j_err <<")." << "\n" << flush;
                *f_stream << "\n";
                *f_stream << "UMatrix2D< FlowNode2D<FP,NUM_COMPONENTS> >" << "\n" << flush;
                f_stream->flush();
                if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                    CloseSwapFile(GasSwapFileName,
                                  GasSwapData,
                                  FileSizeGas,
                                  fd_g,
                                  1);
#endif // _REMOVE_SWAPFILE_
                    useSwapFile = 0;
                } else {
                    delete J;
                }
                J=NULL;
                Abort_OpenHyperFLOW2D();
#ifdef _DEBUG_0
            }__except(Area2D*) {
#endif  // _DEBUG_0
                *f_stream << "\n";
                *f_stream << "Error set computation area state ("<< i_err << "," << j_err <<")." << "\n" << flush;
                f_stream->flush();
                if ( GasSwapData!=0 ) {
#ifdef _REMOVE_SWAPFILE_
                    CloseSwapFile(GasSwapFileName,
                                  GasSwapData,
                                  FileSizeGas,
                                  fd_g,
                                  1);
#endif // _REMOVE_SWAPFILE_
                    useSwapFile = 0;
                } else {
                    delete J;
                }
                J=NULL;
                Abort_OpenHyperFLOW2D();
            }
            __end_except;
#endif  // _DEBUG_0

            //if(GlobalTime > 0.)
            //   J->GetValue(0,0).time = GlobalTime;
            //else
            //   GlobalTime=J->GetValue(0,0).time;

            *f_stream << "\nInitial dt=" << dt << "sec." << endl;
//------> place here <------*
            SetWallNodes(f_stream, J);
            GlobalSubDomain = ScanArea(f_stream,J, isVerboseOutput);
/* Load additional sources */
             isGasSource  = Data->GetIntVal((char*)"NumSrc");
             if ( Data->GetDataError()==-1 ) Abort_OpenHyperFLOW2D();

             if ( isGasSource ) {
                  SrcList = new SourceList2D(J,Data);
                  if ( SrcList) {
                        *f_stream << "\nSet gas sources...";
                        SrcList->SetSources2D();
                        *f_stream << "OK" << endl;
                   }
             }
             
             if(!PreloadFlag) {
                *f_stream << "Seek nodes with non-reflected BC..." << endl;
                *f_stream << "Found " << SetNonReflectedBC(J,nrbc_beta0,f_stream) << " nodes with non-reflected BC...OK" << endl;
             }
             
             if(!PreloadFlag) {
                *f_stream << "Set initial boundary layer...";
                SetInitBoundaryLayer(J,delta_bl);
                *f_stream << "OK" << endl; 
               }

#ifdef _DEBUG_0
        }__except(SysException e) {
            {
                const char ConstErrorMessage[]=" handled by <LibExcept> module in <InitSolver>.\n";
                if ( e == SIGSEGV )
                    *f_stream << "\nSIGSEGV" << ConstErrorMessage << flush;
                else if ( e == SIGBUS )
                    *f_stream << "\nSIGBUS" << ConstErrorMessage << flush;
                else if ( e == SIGFPE )
                    *f_stream << "\nSIGFPE" << ConstErrorMessage << flush;
                else if ( e == SIGINT )
                    *f_stream << "\nSIGINT" << ConstErrorMessage << flush;
                else if ( e == SIGABRT )
                    *f_stream << "\nSIGABRT" << ConstErrorMessage << flush;
                else if ( e == SIGIO )
                    *f_stream << "\nSIGIO" << ConstErrorMessage << flush;
                else if ( e == SIGTRAP )
                    *f_stream << "\nSIGTRAP" << ConstErrorMessage << flush;
                else if ( e == SIGKILL )
                    *f_stream << "\nSIGKILL" << ConstErrorMessage << flush;
                f_stream->flush();
            }
           Abort_OpenHyperFLOW2D();
        } __end_except;
//-- debug ----
//           DataSnapshot(ErrFileName);
//-- debug ----

#endif  // _DEBUG_0
        return(NULL);
};

UArray< XY<int> >*
ScanArea(ofstream* f_str,ComputationalMatrix2D* pJ ,int isPrint) {
    TurbulenceCondType2D TM = TCT_No_Turbulence_2D;
    timeval  time1, time2;
    unsigned int i,j,jj; //ii,iw,jw,
    long num_active_nodes,active_nodes_per_SubDomain;
    UArray< XY<int> >* pWallNodes;
    UArray< XY<int> >* SubDomain;
    pWallNodes = new UArray< XY<int> >();
    SubDomain = new UArray< XY<int> >();
    XY<int> ij;
    XY<int> ijsm;

    num_active_nodes=0;

    if ( isPrint )
        *f_str << "Scan computation area for lookup wall nodes.\n" << flush;
    for ( j=0;j<MaxY;j++ ) {
       for ( i=0;i<MaxX;i++ ) {
           if (!pJ->GetValue(i,j).isCond2D(CT_SOLID_2D)){
               num_active_nodes++;
               pJ->GetValue(i,j).SetCond2D(CT_NODE_IS_SET_2D);
            if ( pJ->GetValue(i,j).isCond2D(CT_WALL_LAW_2D) || 
                 pJ->GetValue(i,j).isCond2D(CT_WALL_NO_SLIP_2D)) {
                ij.SetXY(i,j);
                pWallNodes->AddElement(&ij);
            }
           } 
        }
       if ( isPrint == 1 ) {
           *f_str << "Scan "<< 100*j/MaxY << "% nodes\r"<< flush;
       } else if ( isPrint == 2 ) {
           *f_str << "Scan "<< 100*j/MaxY << "% nodes\n"<< flush;
       }
    }

#ifdef _MPI
num_threads = MPI::COMM_WORLD.Get_size();
#endif // _MPI

#ifdef _OPENMP
#pragma omp parallel
{
 num_threads = omp_get_num_threads(); 
}
#endif // _OPENMP
active_nodes_per_SubDomain = num_active_nodes/num_threads; 
if ( isPrint )
        *f_str << "Found " << pWallNodes->GetNumElements() <<" wall nodes from " << num_active_nodes << " gas filled nodes (" << num_threads <<" subdomains, "<< active_nodes_per_SubDomain <<" active nodes per subdomain).\n" << flush;

    if ( isPrint )
        *f_str << "Scan computation area for lookup minimal distance from internal nodes to wall.\n" << flush;
#ifdef _DEBUG_0 // 2
gettimeofday(&time2,NULL);
#endif // _DEBUG_0 // 2
num_active_nodes=0;
ijsm.SetX(0);

if(isTurbulenceReset) {
   if ( TurbMod == 0 )
        TM = (TurbulenceCondType2D)(TCT_No_Turbulence_2D);
   else if ( TurbMod == 1 )
        TM = (TurbulenceCondType2D)(TCT_Integral_Model_2D);
   else if ( TurbMod == 2 )
        TM = (TurbulenceCondType2D)(TCT_Prandtl_Model_2D);
   else if ( TurbMod == 3 )
        TM = (TurbulenceCondType2D)(TCT_Spalart_Allmaras_Model_2D);
   else if ( TurbMod == 4 )
        TM = (TurbulenceCondType2D)(TCT_k_eps_Model_2D);
   else if ( TurbMod == 5 )
        TM = (TurbulenceCondType2D)(TCT_Smagorinsky_Model_2D);
    if ( isPrint )
        *f_str << "Reset turbulence model to " << PrintTurbCond(TurbMod) << "\n" << flush;
}
#ifdef _OPENMP
#pragma omp for
#endif // _OPENMP
for (int i=0;i<(int)MaxX;i++ ) {
        for (int j=0;j<(int)MaxY;j++ ) {

                    if(isTurbulenceReset) {
                       if(pJ->GetValue(i,j).isTurbulenceCond2D(TCT_Integral_Model_2D))
                          pJ->GetValue(i,j).CleanTurbulenceCond2D(TCT_Integral_Model_2D);
                       if(pJ->GetValue(i,j).isTurbulenceCond2D(TCT_Prandtl_Model_2D))
                          pJ->GetValue(i,j).CleanTurbulenceCond2D(TCT_Prandtl_Model_2D);
                       if(pJ->GetValue(i,j).isTurbulenceCond2D(TCT_Spalart_Allmaras_Model_2D))
                          pJ->GetValue(i,j).CleanTurbulenceCond2D(TCT_Spalart_Allmaras_Model_2D);
                       if(pJ->GetValue(i,j).isTurbulenceCond2D(TCT_k_eps_Model_2D))
                          pJ->GetValue(i,j).CleanTurbulenceCond2D(TCT_k_eps_Model_2D);
                       if(pJ->GetValue(i,j).isTurbulenceCond2D(TCT_Smagorinsky_Model_2D))
                          pJ->GetValue(i,j).CleanTurbulenceCond2D(TCT_Smagorinsky_Model_2D);

                       pJ->GetValue(i,j).SetTurbulenceCond2D(TM);
                       pJ->GetValue(i,j).dkdx = pJ->GetValue(i,j).dkdy = pJ->GetValue(i,j).depsdx = pJ->GetValue(i,j).depsdy =
                       pJ->GetValue(i,j).S[i2d_k] = pJ->GetValue(i,j).S[i2d_eps] =
                       pJ->GetValue(i,j).Src[i2d_k] = pJ->GetValue(i,j).Src[i2d_eps] =
                       pJ->GetValue(i,j).mu_t = pJ->GetValue(i,j).lam_t = 0.0;
                       pJ->GetValue(i,j).FillNode2D(0,1);
                    }

                if (pJ->GetValue(i,j).isCond2D(CT_NODE_IS_SET_2D) && !pJ->GetValue(i,j).isCond2D(CT_SOLID_2D)) {
                    num_active_nodes++;
                    
                    if(num_active_nodes >= active_nodes_per_SubDomain) {
                       ijsm.SetY(i+1);
                       SubDomain->AddElement(&ijsm);
                       ijsm.SetX(i);
                       num_active_nodes = 0;
                    }
                }
        }
#ifndef _OPENMP
   if ( isPrint == 1 && isVerboseOutput ) {
       *f_str << "Scan "<< 100.*i/MaxX+1 << "% nodes   \r"<< flush;
   } else   if ( isPrint == 2 && isVerboseOutput ) {
       *f_str << "Scan "<< 100.*i/MaxX+1 << "% nodes   \n"<< flush;
   }
#endif // _OPENMP

}
#ifdef _DEBUG_0 // 2
            gettimeofday(&time1,NULL);
            *f_str << " Time:" << (time1.tv_sec-time2.tv_sec)+(time1.tv_usec-time2.tv_usec)*1.e-6 << "sec.\n" << flush; 
#endif // _DEBUG_0 // 2
            delete WallNodes;
*f_str << "SubDomain decomposition was finished:\n";
for(jj=0;jj<SubDomain->GetNumElements();jj++) {
   *f_str << "SubDomain[" << jj << "]->["<< SubDomain->GetElementPtr(jj)->GetX() <<","<< SubDomain->GetElementPtr(jj)->GetY() <<"]\n";
}
if(isTurbulenceReset) {
   isTurbulenceReset = 0;
}
f_str->flush();
return SubDomain;
}

void SetInitBoundaryLayer(ComputationalMatrix2D* pJ, FP delta) {
    for (int i=0;i<(int)pJ->GetX();i++ ) {
           for (int j=0;j<(int)pJ->GetY();j++ ) {
                  if (pJ->GetValue(i,j).isCond2D(CT_NODE_IS_SET_2D) &&
                      !pJ->GetValue(i,j).isCond2D(CT_SOLID_2D) &&
                       delta > 0) {
                       if(pJ->GetValue(i,j).l_min <= delta)
                          pJ->GetValue(i,j).S[i2d_RhoU] = pJ->GetValue(i,j).S[i2d_RhoU] * pJ->GetValue(i,j).l_min/delta;
                          pJ->GetValue(i,j).S[i2d_RhoV] = pJ->GetValue(i,j).S[i2d_RhoV] * pJ->GetValue(i,j).l_min/delta;
                          pJ->GetValue(i,j).FillNode2D(0,1);
                   }
           }
    }
}

void RecalcWallFrictionVelocityArray2D(ComputationalMatrix2D* pJ,
                                       UArray<FP>* WallFrictionVelocityArray2D,
                                       UArray< XY<int> >* WallNodes2D) { 
    for(int ii=0;ii<(int)WallNodes2D->GetNumElements();ii++) {
        unsigned int iw,jw;
        FP tau_w;
        iw = WallNodes2D->GetElementPtr(ii)->GetX();
        jw = WallNodes2D->GetElementPtr(ii)->GetY();
        tau_w = (fabs(pJ->GetValue(iw,jw).dUdy)  +
                 fabs(pJ->GetValue(iw,jw).dVdx)) * pJ->GetValue(iw,jw).mu;
        WallFrictionVelocityArray2D->GetElement(ii) = sqrt(tau_w/pJ->GetValue(iw,jw).S[i2d_Rho]+1e-30);
    }
}

UArray<FP>* GetWallFrictionVelocityArray2D(ComputationalMatrix2D* pJ, 
                                               UArray< XY<int> >* WallNodes2D) { 
    UArray<FP>* WallFrictionVelocityArray2D;
    WallFrictionVelocityArray2D = new UArray<FP>();
    for(int ii=0;ii<(int)WallNodes2D->GetNumElements();ii++) {
        unsigned int iw,jw;
        FP tau_w, U_w;
        iw = WallNodes2D->GetElementPtr(ii)->GetX();
        jw = WallNodes2D->GetElementPtr(ii)->GetY();
        tau_w = (fabs(pJ->GetValue(iw,jw).dUdy)  +
                 fabs(pJ->GetValue(iw,jw).dVdx)) * pJ->GetValue(iw,jw).mu;
        U_w   = sqrt(tau_w/pJ->GetValue(iw,jw).S[i2d_Rho]+1e-30);
        WallFrictionVelocityArray2D->AddElement(&U_w);
    }
  return WallFrictionVelocityArray2D;
}

u_long SetWallNodes(ofstream* f_str, ComputationalMatrix2D* pJ) {
u_long NumWallNodes=0;
    for (int j=0;j<(int)pJ->GetY();j++ ) {
       for (int i=0;i<(int)pJ->GetX();i++ ) {

                FlowNode2D< FP,NUM_COMPONENTS >* CurrentNode=NULL;
                FlowNode2D< FP,NUM_COMPONENTS >* UpNode=NULL;
                FlowNode2D< FP,NUM_COMPONENTS >* DownNode=NULL;
                FlowNode2D< FP,NUM_COMPONENTS >* LeftNode=NULL;
                FlowNode2D< FP,NUM_COMPONENTS >* RightNode=NULL;

                CurrentNode = &pJ->GetValue(i,j);

                if(!CurrentNode->isCond2D(CT_SOLID_2D) &&
                   !CurrentNode->isCond2D(NT_FC_2D)) {

                if(j < (int)pJ->GetY()-1)
                  UpNode    = &(pJ->GetValue(i,j+1));
                else
                  UpNode    = NULL;

                if(j>0)
                  DownNode  = &(pJ->GetValue(i,j-1));
                else
                  DownNode  = NULL;

                if(i>0)
                  LeftNode  = &(pJ->GetValue(i-1,j));
                else
                  LeftNode  = NULL;

                if(i < (int)pJ->GetX()-1)
                  RightNode = &(pJ->GetValue(i+1,j));
                else
                  RightNode = NULL;

               if(UpNode && UpNode->isCond2D(CT_SOLID_2D)) {
                   CurrentNode->SetCond2D(NT_WNS_2D);
                   NumWallNodes++;
               } else if(DownNode && DownNode->isCond2D(CT_SOLID_2D)) {
                   CurrentNode->SetCond2D(NT_WNS_2D);
                   NumWallNodes++;
               } else if(LeftNode && LeftNode->isCond2D(CT_SOLID_2D)) {
                   CurrentNode->SetCond2D(NT_WNS_2D);
                   NumWallNodes++;
               } else if(RightNode && RightNode->isCond2D(CT_SOLID_2D)) {
                   CurrentNode->SetCond2D(NT_WNS_2D);
                   NumWallNodes++;
               }
           }
       }
    }
 *f_str << "\nFound and restore " << NumWallNodes << " wall nodes" << endl;
 return  NumWallNodes;
}

UArray< XY<int> >* GetWallNodes(ofstream* f_str, ComputationalMatrix2D* pJ, int isPrint) {
    XY<int> ij;
    UArray< XY<int> >* WallNodes;
    WallNodes = new UArray< XY<int> >();
    if ( isPrint )
        *f_str << "Scan computation area for lookup wall nodes.\n" << flush;
    for (int j=0;j<(int)pJ->GetY();j++ ) {
       for (int i=0;i<(int)pJ->GetX();i++ ) {
           if (!pJ->GetValue(i,j).isCond2D(CT_SOLID_2D)){
            if ( pJ->GetValue(i,j).isCond2D(CT_WALL_LAW_2D) || 
                 pJ->GetValue(i,j).isCond2D(CT_WALL_NO_SLIP_2D)) {
                ij.SetXY(i,j);
                WallNodes->AddElement(&ij);
            }
           } 
        }
       if ( isPrint == 1 )  {
          *f_str << "Scan "<< 100*j/MaxY << "% nodes\r"<< flush; 
       } else if ( isPrint == 2 ) {
          *f_str << "Scan "<< 100*j/MaxY << "% nodes\n"<< flush; 
       }
    }
 return WallNodes;
}
const char* PrintTurbCond(int TM) {
   if ( TM == 0 )
          return "TCT_No_Turbulence_2D";
   else if ( TM == 1 )
          return "TCT_Integral_Model_2D";
   else if ( TM == 2 )
          return "TCT_Prandtl_Model_2D";
   else if ( TM == 3 )
          return "TCT_Spalart_Allmaras_Model_2D";
   else if ( TM == 4 )
          return "TCT_k_eps_Model_2D";
   else if ( TM == 5 )
          return "TCT_Smagorinsky_Model_2D";
   return "Unknown turbulence model";
}

void PrintCond(ofstream* OutputData, FlowNode2D<FP,NUM_COMPONENTS>* fn) {
    //Const conditions

    if ( fn->isCond2D(CT_Rho_CONST_2D) )
        *OutputData << " CT_Rho_CONST_2D" << endl << flush;
    if ( fn->isCond2D(CT_U_CONST_2D) )
        *OutputData << " CT_U_CONST_2D"   << endl << flush;
    if ( fn->isCond2D(CT_V_CONST_2D) )
        *OutputData << " CT_V_CONST_2D"   << endl << flush;
    if ( fn->isCond2D(CT_T_CONST_2D) )
        *OutputData << " CT_T_CONST_2D"   << flush;
    if ( fn->isCond2D(CT_Y_CONST_2D) )
        *OutputData << " CT_Y_CONST_2D"   << flush;

    // dF/dx = 0
    if ( fn->isCond2D(CT_dRhodx_NULL_2D) )
        *OutputData << " CT_dRhodx_NULL_2D"<< endl  << flush;
    if ( fn->isCond2D(CT_dUdx_NULL_2D) )
        *OutputData << " CT_dUdx_NULL_2D" << endl << flush;
    if ( fn->isCond2D(CT_dVdx_NULL_2D) )
        *OutputData << " CT_dVdx_NULL_2D" << endl << flush;
    if ( fn->isCond2D(CT_dTdx_NULL_2D) )
        *OutputData << " CT_dTdx_NULL_2D" << endl << flush;
    if ( fn->isCond2D(CT_dYdx_NULL_2D) )
        *OutputData << " CT_dYdx_NULL_2D" << endl << flush;

    // dF/dy = 0

    if ( fn->isCond2D(CT_dRhody_NULL_2D) )
        *OutputData << " CT_dRhody_NULL_2D"<< endl << flush;
    if ( fn->isCond2D(CT_dUdy_NULL_2D) )
        *OutputData << " CT_dUdy_NULL_2D" << endl << flush;
    if ( fn->isCond2D(CT_dVdy_NULL_2D) )
        *OutputData << " CT_dVdy_NULL_2D" << endl << flush;
    if ( fn->isCond2D(CT_dTdy_NULL_2D) )
        *OutputData << " CT_dTdy_NULL_2D" << endl << flush;
    if ( fn->isCond2D(CT_dYdy_NULL_2D) )
        *OutputData << " CT_dYdy_NULL_2D" << endl << flush;

    // d2F/dx2 =
    if ( fn->isCond2D(CT_d2Rhodx2_NULL_2D) )
        *OutputData << " CT_d2Rhodx2_NULL_2D" << endl << flush;
    if ( fn->isCond2D(CT_d2Udx2_NULL_2D) )
        *OutputData << " CT_d2Udx2_NULL_2D"   << endl << flush;
    if ( fn->isCond2D(CT_d2Vdx2_NULL_2D) )
        *OutputData << " CT_d2Vdx2_NULL_2D"   << endl << flush;
    if ( fn->isCond2D(CT_d2Tdx2_NULL_2D) )
        *OutputData << " CT_d2Tdx2_NULL_2D"   << endl << flush;
    if ( fn->isCond2D(CT_d2Ydx2_NULL_2D) )
        *OutputData << " CT_d2Ydx2_NULL_2D"   << endl << flush;

    // d2F/dy2 = 0

    if ( fn->isCond2D(CT_d2Rhody2_NULL_2D) )
        *OutputData << " CT_d2Rhody2_NULL_2D" << endl << flush;
    if ( fn->isCond2D(CT_d2Udy2_NULL_2D) )
        *OutputData << " CT_d2Udy2_NULL_2D"   << endl << flush;
    if ( fn->isCond2D(CT_d2Vdy2_NULL_2D) )
        *OutputData << " CT_d2Vdy2_NULL_2D"   << endl << flush;
    if ( fn->isCond2D(CT_d2Tdy2_NULL_2D) )
        *OutputData << " CT_d2Tdy2_NULL_2D"   << endl << flush;
    if ( fn->isCond2D(CT_d2Ydy2_NULL_2D) )
        *OutputData << " CT_d2Ydy2_NULL_2D"   << endl << flush;

    // Wall conditions:
    // - Slip
    if ( fn->isCond2D(CT_WALL_LAW_2D) )
        *OutputData << " CT_WALL_LAW_2D"      << endl << flush;
    // - Glide
    if ( fn->isCond2D(CT_WALL_NO_SLIP_2D) )
        *OutputData << " CT_WALL_NO_SLIP_2D"  << endl << flush;
    // is solid body ?
    if ( fn->isCond2D(CT_SOLID_2D) )
        *OutputData << " CT_SOLID_2D"         << endl << flush;
    // is node activate ?
    if ( fn->isCond2D(CT_NODE_IS_SET_2D) )
        *OutputData << " CT_NODE_IS_SET_2D"   << endl << flush;
    // boundary layer refinement
    if ( fn->isCond2D(CT_BL_REFINEMENT_2D) )
        *OutputData << " CT_BL_REFINEMENT_2D" << endl << flush;
    
    // Special BC
    // non-reflected BC
    if ( fn->isCond2D(CT_NONREFLECTED_2D) )
        *OutputData << " CT_NONREFLECTED_2D" << endl << flush;

}


void DataSnapshot(char* filename, WRITE_MODE ioMode) {
#ifdef _DEBUG_0
    static SysException E=0;
    ___try {
#endif  // _DEBUG_0
        if ( ioMode ) // 0 - append(TecPlot), 1- rewrite(GNUPlot)
            CutFile(filename);
        pOutputData = OpenData(filename);
        SaveData2D(pOutputData,OutVariables_Array,ioMode);
#ifdef _DEBUG_0
    } __except(SysException e) {
        E=e;
    }
    __end_except;
#endif // _DEBUG_0
    pOutputData ->close();
#ifdef _DEBUG_0
    if ( E )
        throw(E);
    else
#endif // _DEBUG_0
        return;
}

ofstream* OpenData(char* outputDataFile) {
    static ofstream* pOutputData;
    pOutputData = new ofstream();
    pOutputData->open(outputDataFile, ios::app);
    return(pOutputData);
}

void CutFile(char* cutFile) {
    static ofstream* pCutFile;
    pCutFile = new ofstream();
    pCutFile->open(cutFile, ios::trunc);
    pCutFile->close();
}

void SaveMonitorsHeader(ofstream* MonitorsFile,UArray< MonitorPoint >* MonitorPtArray) {
    char  TecPlotTitle[1024]={'\0'};
    char  MonitorStr[256];
    snprintf(TecPlotTitle,1024,"#VARIABLES = Time");
    
    for (int i=0;i<(int)MonitorPtArray->GetNumElements();i++) {
         // Point-%i.U, Point-%i.V, 
         snprintf(MonitorStr,256,", Point-%i.p, Point-%i.T",i+1,i+1);
         strcat(TecPlotTitle,MonitorStr);
    }
   *MonitorsFile << TecPlotTitle << endl;
}

void SaveMonitors(ofstream* MonitorsFile, 
                  FP t, 
                  UArray< MonitorPoint >* MonitorPtArray) {
    *MonitorsFile  << t << " ";
    for (int i=0;i<(int)MonitorPtArray->GetNumElements();i++) {
        // MonitorPtArray->GetElement(i).MonitorNode.U << " " << MonitorPtArray->GetElement(i).MonitorNode.V << " " <<
        *MonitorsFile << MonitorPtArray->GetElement(i).p << " " << MonitorPtArray->GetElement(i).T << " ";
    }
    *MonitorsFile << endl;
}
#ifdef _RMS_
void SaveRMSHeader(ofstream* OutputData) {
        char  TecPlotTitle[1024];
        char  TmpData[256]={'\0'};

        if(is_Cx_calc)
           snprintf(TmpData,256,", Cx(N), Cy(N), Fx(N), Fy(N)\n");

        snprintf(TecPlotTitle,1024,"#VARIABLES = N, RMS(Rho), RMS(Rho*U), RMS(Rho*V), RMS(Rho*E), RMS(Rho*Y_fu), RMS(Rho*Y_ox), RMS(RhoY*cp), RMS(k), RMS(eps)%s",TmpData);

        *OutputData <<  TecPlotTitle << endl;
    }

    void SaveRMS(ofstream* OutputData,unsigned int n, float* outRMS) {
         *OutputData <<  n  << " ";
         for(int i=0;i<FlowNode2D<FP,NUM_COMPONENTS>::NumEq;i++) {
             *OutputData <<  outRMS[i]  << " ";
         }

        if(is_Cx_calc) {
          *OutputData << Calc_Cx_2D(J,x0_body,y0_body,dx_body,dy_body,Flow2DList->GetElement(Cx_Flow_index-1)) << " " <<  Calc_Cy_2D(J,x0_body,y0_body,dx_body,dy_body,Flow2DList->GetElement(Cx_Flow_index-1)) << " ";
          *OutputData << CalcXForce2D(J,x0_body,y0_body,dx_body,dy_body) << " " <<  CalcYForce2D(J,x0_body,y0_body,dx_body,dy_body);
        }

        *OutputData << endl;
    }
#endif // _RMS_
    void SaveData2D(ofstream* OutputData,
                    UArray<OutVariable>* OutVariablesArray, 
                    int type) { // type = 1 - GNUPLOT
        int    i,j;
        char   TechPlotTitle1[1024]={0};
        char   TechPlotTitle2[256]={0};
        char   YR[2];
        char   HT[2];

        FlowNode2D<FP,NUM_COMPONENTS>* CurrentNode = NULL;
        FP     Cp_wall = 0.0;
        FP Mach,A,W,Re,Re_t,dx_out,dy_out;
        
        int NUM_VARS=2; // X,Y = 0,1
        
        char  VarList[512];

        if(!OutVariablesArray || OutVariablesArray->GetNumElements() == 0) {
           cout << "Variables list empthy !" << endl;
           return;
        }

        if(FlowNode2D<FP,NUM_COMPONENTS>::FT == 1) // FT_FLAT
          snprintf(YR,2,"R");
        else
          snprintf(YR,2,"Y");
        
        if (type == 1) {
           snprintf(HT,2,"# ");
        } else {
           snprintf(HT,2," ");
        }

        dx_out =(dx*MaxX)/(MaxX-1); 
        dy_out =(dy*MaxY)/(MaxY-1); 
        
        memset(VarList,0,512);

        for (int i=0; i < OutVariablesArray->GetNumElements();i++) {
            if(strlen(VarList)+strlen(OutVariablesArray->GetElementPtr(i)->VarName) + 2 < 512) {
                strcat(VarList,OutVariablesArray->GetElementPtr(i)->VarName);
                if (i < OutVariablesArray->GetNumElements()-1) {
                    strcat(VarList,", ");
                }
             NUM_VARS++;
            }
        }

        snprintf(TechPlotTitle1,1024,"%sVARIABLES = X, %s, %s"
                                     "\n",HT, YR,VarList); 
        
        snprintf(TechPlotTitle2,256,"%sZONE T=\"Time: %g sec.\" I= %i J= %i F=POINT\n",HT, GlobalTime, MaxX, MaxY);

        if ( type ) {
            *OutputData << TechPlotTitle1;
            *OutputData << TechPlotTitle2;
        } else {
            *OutputData <<  TechPlotTitle1;
            *OutputData <<  TechPlotTitle2;
        }
        

         for (int j=0;j<(int)MaxY;j++ ) {
             for (int  i=0;i<(int)MaxX;i++ ) {
                 for (int ii=0;ii<NUM_VARS;ii++) {
                     if (ii == 0) {
                         *OutputData << i*dx_out*1.e3                                                           << "  "; // x
                     } else if (ii == 1) {
                         *OutputData << dy_out*j*1.e3                                                           << "  "; // y
                     } else {
                         if (J->GetValue(i,j).isCond2D(CT_NODE_IS_SET_2D) &&
                             !J->GetValue(i,j).isCond2D(CT_SOLID_2D)) {
                             *OutputData << OutVar(&J->GetValue(i,j),OutVariablesArray->GetElement(ii-2).VarID) << "  "; // Var
                         } else {
                             *OutputData << "0  ";
                         }
                     }
                 }
                 *OutputData <<  "\n" ;
             }
            if ( type )
                *OutputData <<  "\n" ;
         }

        /*
        for ( j=0;j<(int)MaxY;j++ ) {
            for ( i=0;i<(int)MaxX;i++ ) {
                *OutputData << i*dx_out*1.e3                    << "  "; // 1
                *OutputData << dy_out*j*1.e3                    << "  "; // 2
                CurrentNode = &J->GetValue(i,j);
                Mach = Re = Re_t = 0;
                if ( !J->GetValue(i,j).isCond2D(CT_SOLID_2D) ) {
                    *OutputData << J->GetValue(i,j).U           << "  "; // 3
                    *OutputData << J->GetValue(i,j).V           << "  "; // 4
                    *OutputData << J->GetValue(i,j).Tg          << "  "; // 5
                    *OutputData << J->GetValue(i,j).p           << "  "; // 6
                    *OutputData << J->GetValue(i,j).S[0]        << "  "; // 7

                    if(CurrentNode && 
                       Flow2DList  &&
                       Cx_Flow_index > 0)
                    Cp_wall = Calc_Cp(CurrentNode,Flow2DList->GetElement(Cx_Flow_index-1));
                    
                    A = sqrt(J->GetValue(i,j).k*J->GetValue(i,j).R*J->GetValue(i,j).Tg+1.e-30);
                    W = sqrt(J->GetValue(i,j).U*J->GetValue(i,j).U+J->GetValue(i,j).V*J->GetValue(i,j).V+1.e-30);
                    Mach = W/A;
                    
                    if ( J->GetValue(i,j).S[0] != 0. ) {
                        *OutputData << J->GetValue(i,j).S[4]/J->GetValue(i,j).S[0] << "  ";  // 8
                        *OutputData << J->GetValue(i,j).S[5]/J->GetValue(i,j).S[0] << "  ";  // 9
                        *OutputData << J->GetValue(i,j).S[6]/J->GetValue(i,j).S[0] << "  ";  // 10
                        *OutputData << fabs(1-J->GetValue(i,j).S[4]/J->GetValue(i,j).S[0]-J->GetValue(i,j).S[5]/J->GetValue(i,j).S[0]-J->GetValue(i,j).S[6]/J->GetValue(i,j).S[0]) << "  "; //11

                        if(is_p_asterisk_out)
                          *OutputData << p_asterisk(&(J->GetValue(i,j))) << "  ";            // 12
                        else
                          *OutputData << J->GetValue(i,j).mu_t/J->GetValue(i,j).mu << "  ";  // 12
                        
                    } else {
                        *OutputData << " +0. +0  +0  +0  +0  "; 
                    }
                } else {
                    *OutputData << "  0  0  ";                    
                    *OutputData << J->GetValue(i,j).Tg;           
                    *OutputData << "  0  0  0  0  0  0  0";       
                }
                if(!J->GetValue(i,j).isCond2D(CT_SOLID_2D)) {
                    if( Mach > 1.e-30) 
                      *OutputData << Mach  << "  " << J->GetValue(i,j).l_min  << " " << J->GetValue(i,j).dt_local;  
                    else
                      *OutputData << "  0  0  0";
                } else {
                    *OutputData << "  0  0  0";
                }
                *OutputData <<  "\n" ;
            }
            if ( type )
                *OutputData <<  "\n" ;
        }
     */
    }

    inline FP kg(FP Cp, FP R) {
        return(Cp/(Cp-R));
    }

