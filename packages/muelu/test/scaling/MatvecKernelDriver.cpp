// @HEADER
//
// ***********************************************************************
//
//        MueLu: A package for multigrid based preconditioning
//                  Copyright 2012 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact
//                    Jonathan Hu       (jhu@sandia.gov)
//                    Andrey Prokopenko (aprokop@sandia.gov)
//                    Ray Tuminaro      (rstumin@sandia.gov)
//
// ***********************************************************************
//
// @HEADER
#include <iostream>
#include <algorithm>  // shuffle
#include <vector>     // vector
#include <random>

#include <Xpetra_MultiVectorFactory.hpp>
#include <Xpetra_IO.hpp>

// Teuchos
#include <Teuchos_StandardCatchMacros.hpp>
#include <Teuchos_StackedTimer.hpp>

// Galeri
#include <Galeri_XpetraParameters.hpp>
#include <Galeri_XpetraProblemFactory.hpp>
#include <Galeri_XpetraUtils.hpp>
#include <Galeri_XpetraMaps.hpp>
// MueLu
#include "MueLu.hpp"
#include "MueLu_TestHelpers.hpp"
#include "MueLu_PerfModels.hpp"
#include <MatrixLoad.hpp>

#if defined(HAVE_MUELU_TPETRA)
#include "Xpetra_TpetraMultiVector.hpp"
#include "Xpetra_TpetraImport.hpp"
#include "Tpetra_CrsMatrix.hpp"
#include "Tpetra_MultiVector.hpp"
#include "KokkosSparse_spmv.hpp"
#endif
#include "Kokkos_Core.hpp"

#if defined(HAVE_MUELU_CUSPARSE)
#include "cublas_v2.h"
#include "cusparse.h"
#endif

#if defined (HAVE_MUELU_HYPRE)
#include "HYPRE_IJ_mv.h"
#include "HYPRE_parcsr_ls.h"
#include "HYPRE_parcsr_mv.h"
#include "HYPRE.h"
#endif

#if defined (HAVE_MUELU_PETSC)
#include "petscksp.h"
#endif


Teuchos::RCP<Teuchos::StackedTimer> stacked_timer;
Teuchos::RCP<Teuchos::TimeMonitor> globalTimeMonitor;

// =========================================================================
// Support Routines
// =========================================================================


template<class View1, class View2>
inline void copy_view_n(const int n, const View1 x1, View2 x2) {
  Kokkos::parallel_for(n,KOKKOS_LAMBDA(const size_t i) {
      x2[i] = x1[i];
    });
}

template<class View1, class View2>
inline void copy_view(const View1 x1, View2 x2) {
  copy_view_n(x1.extent(0),x1,x2);
}



template <class V1, class V2>
void print_crs_graph(std::string name, const V1 rowptr, const V2 colind) {
  printf("%s rowptr[%d] = ",name.c_str(),rowptr.extent(0));
  for(size_t i=0; i<rowptr.extent(0); i++)
    printf(" %d",(int)rowptr[i]);
  printf("\n%s colind[%d] = ",name.c_str(),colind.extent(0));
  for(size_t i=0; i<colind.extent(0); i++)
    printf(" %d",(int)colind[i]);
  printf("\n");
}

// =========================================================================
// Performance Routines
// =========================================================================
// Report bandwidth in GB / sec
const double GB = 1024.0 * 1024.0 * 1024.0;

double convert_time_to_bandwidth_gbs(double time, int num_calls, double memory_per_call_bytes) {

  double time_per_call = time / num_calls;

  return memory_per_call_bytes / GB / time_per_call;
}



template<class Matrix>
void report_performance_models(const Teuchos::RCP<const Matrix> & A, int nrepeat, bool verbose) {  
  using Teuchos::RCP;
  const RCP<const Teuchos::Comm<int> > comm = A->getMap()->getComm();
  using SC = typename Matrix::scalar_type;
  using LO = typename Matrix::local_ordinal_type;
  using GO = typename Matrix::global_ordinal_type;
  using NO = typename Matrix::node_type;

  // NOTE: We've hardwired this to size_t for the rowptr.  This really should really get read out of a typedef,
  // if Tpetra actually had one
  using rowptr_type = size_t;
  MueLu::PerfModels<SC,LO,GO,NO> PM;
  int rank = comm->getRank(); int nproc = comm->getSize();
  int m   = static_cast<int>(A->getLocalNumRows());
  int n   = static_cast<int>(A->getColMap()->getLocalNumElements());
  int nnz = static_cast<int>(A->getLocalMatrixHost().graph.entries.extent(0));
  
  // Generate Lookup Tables  
  int v_log_max = ceil(log(nnz) / log(2))+1;
  PM.stream_vector_make_table(nrepeat,v_log_max);

  int m_log_max = 15;
  PM.pingpong_make_table(nrepeat,m_log_max,comm);


  if(A->hasCrsGraph()) {
    auto importer = A->getCrsGraph()->getImporter();
    size_t recv_size = importer->getRemoteLIDs().size() * sizeof(SC);
    size_t send_size = importer->getExportLIDs().size() * sizeof(SC);
    int local_log_max = ceil(log(std::max(send_size,recv_size)) / log(2))+1;
    int global_log_max=local_log_max;
    Teuchos::reduceAll<int>(*comm,Teuchos::REDUCE_MAX,1,&local_log_max,&global_log_max);
    PM.halopong_make_table(nrepeat,global_log_max, importer);   
  }

  if(verbose && rank == 0) {
    std::cout<<"********************************************************"<<std::endl;
    std::cout<<"Performance model results on "<<nproc<<" ranks"<<std::endl;
    std::cout<<"****** Launch Latency Table ******"<<std::endl;
    PM.print_launch_latency_table(std::cout);
    std::cout<<"****** Stream Table ******"<<std::endl;
    PM.print_stream_vector_table(std::cout);
    std::cout<<"****** Latency Corrected Stream Table ******"<<std::endl;
    PM.print_latency_corrected_stream_vector_table(std::cout);
    std::cout<<"****** Pingpong Table ******"<<std::endl;
    PM.print_pingpong_table(std::cout);
    std::cout<<"****** Halopong Table ******"<<std::endl;
    PM.print_halopong_table(std::cout);
  }

  // For convenience
  const int NUM_TIMERS = 6;
  std::string SPMV_test_names[NUM_TIMERS] = {"colind","rowptr","vals","x","y","all"};
  std::vector<int> SPMV_num_objects(NUM_TIMERS), SPMV_object_size(NUM_TIMERS), SPMV_corrected(NUM_TIMERS);


  // Composite model: Use latency correction
  SPMV_num_objects[0] = nnz;     SPMV_object_size[0] = sizeof(LO);           SPMV_corrected[0] = 1;// colind
  SPMV_num_objects[1] = (m + 1); SPMV_object_size[1] = sizeof(rowptr_type);  SPMV_corrected[1] = 1;// rowptr 
  SPMV_num_objects[2] = nnz;     SPMV_object_size[2] = sizeof(SC);           SPMV_corrected[2] = 1;// vals
  SPMV_num_objects[3] = n;       SPMV_object_size[3] = sizeof(SC);           SPMV_corrected[3] = 1; // x
  SPMV_num_objects[4] = m;       SPMV_object_size[4] = sizeof(SC);           SPMV_corrected[4] = 1;// y

  // All-Model: Do not use latency correction
  SPMV_object_size[5] = 1;
  SPMV_num_objects[5]  = (m+1)*sizeof(rowptr_type) + nnz*sizeof(LO) + nnz*sizeof(SC) + 
    n*sizeof(SC) + m*sizeof(SC);  
  SPMV_corrected[5] = 0;


  std::vector<double> gb_per_sec(NUM_TIMERS);
  if(verbose && rank == 0)
    std::cout<<"****** Local Time Model Results ******"<<std::endl;
  for(int i = 0; i < NUM_TIMERS; i++) {
    double avg_time;

    // Table interpolation - Take the faster of the two
    int size_in_bytes = SPMV_object_size[i] *SPMV_num_objects[i];
    if(SPMV_corrected[i] == 1)
      avg_time = PM.latency_corrected_stream_vector_lookup(size_in_bytes);
    else
      avg_time = PM.stream_vector_lookup(size_in_bytes);
    double avg_distributed = avg_time;      

    if(nproc > 1) {
      Teuchos::reduceAll(*comm, Teuchos::REDUCE_SUM, 1, &avg_time, &avg_distributed);
      avg_distributed /= nproc;
    }
    
    // The lookup divides by transactions-per-element already
    double memory_traffic = (double)SPMV_object_size[i] *(double)SPMV_num_objects[i];
    gb_per_sec[i] = convert_time_to_bandwidth_gbs(avg_distributed,1,memory_traffic);
    
    if(verbose && rank == 0) {

      std::cout<< "Local: "<<SPMV_test_names[i] << " # Scalars = "<<memory_traffic/sizeof(SC) << " time per call = "<<avg_distributed*1e6 << " us. GB/sec = "<<gb_per_sec[i]<<std::endl;
    }
  }
  
  // Get the latency info
  double avg_latency;
  if(nproc > 1) {
    double avg_latency_local = PM.launch_latency_lookup();    
    double avg_latency_distributed = avg_latency_local;
    Teuchos::reduceAll(*comm, Teuchos::REDUCE_SUM, 1, &avg_latency_local, &avg_latency_distributed);
    avg_latency  = avg_latency_distributed / nproc;
  }
  else {
    avg_latency = PM.launch_latency_lookup();
  }


  /***************************************************************************/
  // *** Calculate SPMV minimum time (composite) ***
  // Model: 
  // rowptr = One read per row
  // colind = One read per entry
  // values = One read per entry
  // x      = One read per entry in values array (but we assume the cache will work its magic here)
  // y      = One write per row
 
  long unsigned int spmv_memory_bytes[NUM_TIMERS] = {
     (m+1) * sizeof(rowptr_type), //rowptr
     nnz * sizeof(LO), //colind
     nnz * sizeof(SC), // values
     n  * sizeof(SC), //x
     m * sizeof(SC), // y
     0
    };
  for(int i=0; i<NUM_TIMERS-1; i++)
    spmv_memory_bytes[NUM_TIMERS-1] += spmv_memory_bytes[i];


  double minimum_local_composite_time = avg_latency;
  for(int i=0; i<NUM_TIMERS-1; i++)
    minimum_local_composite_time +=  spmv_memory_bytes[i] / (GB * gb_per_sec[i]);

  double minimum_local_all_time = spmv_memory_bytes[NUM_TIMERS-1] / (GB * gb_per_sec[NUM_TIMERS-1]);

  /***************************************************************************/
  // *** Calculate Remote part of the SPMV ***
  double time_pack_unpack_outofplace = 0.0;
  double time_pack_unpack_inplace    = 0.0;
  double time_communicate_ping       = 0.0;
  double time_communicate_halo       = 0.0;
  // Note: We'll assume that each of the permutes, remotes and exports is a unified
  // memory transaction, even though that's not strictly speaking correct.
  if(A->hasCrsGraph()) {
    auto importer = A->getCrsGraph()->getImporter();
    if(! importer.is_null()) {
      // Sames [pack] - 1 read SC, 1 write SC 
      // NOTE: Only if you're out-of-place
      size_t num_sames = importer->getNumSameIDs();
      double same_time = (num_sames == 0) ? 0.0 : 
        2.0 * PM.latency_corrected_stream_vector_lookup(num_sames*sizeof(SC)) + avg_latency;

      // Permutes [pack] - 2 reads LO [to, from] , 1 read SC [values], 1 write SC [values]
      size_t num_permutes = importer->getNumPermuteIDs();
      double permute_time = (num_permutes == 0) ? 0.0 : 
        2.0 * PM.latency_corrected_stream_vector_lookup(num_permutes*sizeof(LO)) + 
        2.0 * PM.latency_corrected_stream_vector_lookup(num_permutes*sizeof(SC)) + avg_latency;

      // Exports [pack] - 1 read LO [exportLIDs], 1 read SC [values], 1 write SC [buffer]
      // This is what Epetra does at least      
      size_t num_exports = importer->getNumExportIDs();
      double export_time = (num_exports == 0) ? 0.0 :
        PM.latency_corrected_stream_vector_lookup(num_exports*sizeof(LO)) + 
        2.0 * PM.latency_corrected_stream_vector_lookup(num_exports*sizeof(SC)) + avg_latency;

      // Remotes [unpack] - 1 read LO [remoteLIDs],  1 read SC [buffer], 1 write SC [values]
      // NOTE: Only if you're out of place
      size_t num_remotes = importer->getNumRemoteIDs();
      double remote_time = (num_remotes == 0) ? 0.0 :
        PM.latency_corrected_stream_vector_lookup(num_remotes*sizeof(LO)) + 
        2.0 * PM.latency_corrected_stream_vector_lookup(num_remotes*sizeof(SC)) + avg_latency;


      // Total pack / unpack time
      time_pack_unpack_outofplace = same_time + permute_time + export_time + remote_time;
      time_pack_unpack_inplace    = permute_time + export_time;

      // We now need to get the size of each message for the ping-pong costs.
      double send_time = 0.0;
      double recv_time = 0.0;
      double halo_time = 0.0;
      size_t total_send_length=0, total_recv_length=0;
      double avg_size_per_msg = 0.0;
#if defined(HAVE_MUELU_TPETRA)
      RCP<const Xpetra::TpetraImport<LO,GO,NO> > t_importer = Teuchos::rcp_dynamic_cast<const Xpetra::TpetraImport<LO,GO,NO> >(importer);
      if(!t_importer.is_null()) {
        RCP<const Tpetra::Import<LO,GO,NO> > tt_i  = t_importer->getTpetra_Import();
        Tpetra::Distributor & distor = tt_i->getDistributor();
        Teuchos::ArrayView<const size_t> recv_lengths = distor.getLengthsFrom();
        Teuchos::ArrayView<const size_t> send_lengths = distor.getLengthsTo();
        
        for (int i=0; i<(int) send_lengths.size(); i++) {
          send_time += PM.pingpong_device_lookup(send_lengths[i] * sizeof(SC));
          total_send_length = send_lengths[i]*sizeof(SC);
        }
        
        for (int i=0; i<(int) recv_lengths.size(); i++)  {
          recv_time += PM.pingpong_device_lookup(recv_lengths[i] * sizeof(SC));                  
          total_recv_length = send_lengths[i]*sizeof(SC);
        }
 
        avg_size_per_msg = (double)total_send_length/(2.0*send_lengths.size()) +  (double)total_recv_length/(2.0*recv_lengths.size());
        halo_time = PM.halopong_device_lookup(avg_size_per_msg);
     }
#endif


      if(verbose && rank == 0) {
         std::cout<<"****** Remote Time Model Results ******"<<std::endl;
        std::cout << "Remote: same     = "<<same_time*1e6<<" us.\n"
                  << "Remote: permutes = "<<permute_time*1e6<<" us.\n"
                  << "Remote: exports  = "<<export_time*1e6<<" us.\n"
                  << "Remote: remotes  = "<<remote_time*1e6<<" us.\n"
                  << "Remote: sends len = "<<total_send_length<<" time = "<<send_time*1e6<<" us.\n"
                  << "Remote: recvs len = "<<total_recv_length<<" time  = "<<recv_time*1e6<<" us.\n"
                  << "Remote: halo avg = "<<(size_t)avg_size_per_msg<<" time  = "<<halo_time*1e6<<" us.\n"<<std::endl;
      }

      // NOTE: For now we'll do comm time as the larger of send/recv.  Not sure this is
      // really the optimal thing to do, but we'll start here.
      time_communicate_ping = std::max(send_time,recv_time);
      time_communicate_halo = halo_time;
    }
  }
  double minimum_time_in_place_ping     = time_communicate_ping + time_pack_unpack_inplace;
  double minimum_time_out_of_place_ping = time_communicate_ping + time_pack_unpack_outofplace;
  double minimum_time_in_place_halo     = time_communicate_halo + time_pack_unpack_inplace;
  double minimum_time_out_of_place_halo = time_communicate_halo + time_pack_unpack_outofplace;


  /***************************************************************************/
  if(rank == 0)
    std::cout << "\n\n========================================================\n"
              << "Minimum time model (composite) : " << minimum_local_composite_time << std::endl
              << "Minimum time model (all)       : " << minimum_local_all_time << std::endl
              << "Pack/unpack in-place           : " << time_pack_unpack_inplace << std::endl
              << "Pack/unpack out-of-place       : " << time_pack_unpack_outofplace << std::endl
              << "Communication time (ping)      : " << time_communicate_ping << std::endl
              << "Communication time (halo)      : " << time_communicate_halo << std::endl;
  

  // Iterate through all of the "MV" timers
  // NOTE: This is a hack since TimeMonitor does not give you a way to
  // iterate through the timers
  std::vector<const char *> timer_names = {"MV MKL: Total",
                                           "MV KK: Total",
                                           "MV Tpetra: Total",
                                           "MV CuSparse: Total",
                                           "MV MagmaSparse: Total",
                                           "MV HYPRE: Total",
                                           "MV Petsc: Total"};

  if(rank == 0) {
    if(!globalTimeMonitor.is_null()) {
      const std::string l[10]={"Comp","Comp+ping+inplace","Comp+ping+ooplace","Comp+halo+inplace","Comp+halo+ooplace",
                                    "All", "All+ping+inplace", "All+ping+ooplace", "All+halo+inplace", "All+halo+ooplace"};
      const std::string div={"-------------------"};
      printf("%-60s %20s %20s %20s %20s %20s %20s %20s %20s %20s %20s\n","Timer",l[0].c_str(),l[1].c_str(),l[2].c_str(),l[3].c_str(),l[4].c_str(),
             l[5].c_str(),l[6].c_str(),l[7].c_str(),l[8].c_str(),l[9].c_str());
      printf("%-60s %20s %20s %20s %20s %20s %20s %20s %20s %20s %20s\n","-----",div.c_str(),div.c_str(),div.c_str(),div.c_str(),div.c_str(),
             div.c_str(),div.c_str(),div.c_str(),div.c_str(),div.c_str());
      for(int i=0; i<(int)timer_names.size(); i++) {
        Teuchos::RCP<Teuchos::Time> t = globalTimeMonitor->lookupCounter(timer_names[i]);
        if(!t.is_null()) {
          double time_per_call = t->totalElapsedTime() / t->numCalls();
          double comp    = minimum_local_composite_time / time_per_call;
          double alls     = minimum_local_all_time / time_per_call;
          double p_inplace = minimum_time_in_place_ping / time_per_call;
          double p_ooplace = minimum_time_out_of_place_ping / time_per_call;
          double h_inplace = minimum_time_in_place_halo / time_per_call;
          double h_ooplace = minimum_time_out_of_place_halo / time_per_call;

          printf("%-60s %20.2f %20.2f %20.2f %20.2f %20.2f %20.2f %20.2f %20.2f %20.2f %20.2f\n",timer_names[i],
                 comp,comp+p_inplace,comp+p_ooplace,comp+h_inplace,comp+h_ooplace,
                 alls,alls+p_inplace,alls+p_ooplace,alls+h_inplace,alls+h_ooplace);
        }
      }
    }
    else {
      std::cout<<"Note: Minimum time model individual timers only work with stacked timers off."<<std::endl;
    }
  }

}
  

#if defined(HAVE_MUELU_TPETRA)
//==============================================================================
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
void MakeContiguousMaps(const Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> & Matrix,
                        Teuchos::RCP<const Tpetra::Map<LocalOrdinal,GlobalOrdinal,Node> > & ContiguousRowMap,
                        Teuchos::RCP<const Tpetra::Map<LocalOrdinal,GlobalOrdinal,Node> > & ContiguousColumnMap,
                        Teuchos::RCP<const Tpetra::Map<LocalOrdinal,GlobalOrdinal,Node> > & ContiguousDomainMap) {
  using Teuchos::rcp;
  using Teuchos::RCP;
  using LO             = LocalOrdinal;
  using GO             = GlobalOrdinal;
  using NO             = Node;
  using map_type       = Tpetra::Map<LO,GO,NO>;
  using import_type    = Tpetra::Import<LO,GO,NO>;
  using go_vector_type = Tpetra::Vector<GO,LO,GO,NO>;


  // Must create GloballyContiguous DomainMap (which is a permutation of Matrix_'s
  // DomainMap) and the corresponding permuted ColumnMap.
  //   Epetra_GID  --------->   LID   ----------> HYPRE_GID
  //           via DomainMap.LID()       via GloballyContiguousDomainMap.GID()
  RCP<const map_type> RowMap      = Matrix.getRowMap();
  RCP<const map_type> DomainMap   = Matrix.getDomainMap();
  RCP<const map_type> ColumnMap   = Matrix.getColMap();
  RCP<const import_type> importer = Matrix.getGraph()->getImporter();

  if(RowMap->isContiguous() ) {
    // If the row map is linear, then we can just use the row map as is.
    ContiguousRowMap = RowMap;
  }
  else {
    // The row map isn't linear, so we need a new row map
    ContiguousRowMap = rcp(new map_type(Matrix.getRowMap()->getGlobalNumElements(),
                                        Matrix.getRowMap()->getLocalNumElements(), 0, Matrix.getRowMap()->getComm()));
  }


  if(DomainMap->isContiguous() ) {
    // If the domain map is linear, then we can just use the column map as is.
    ContiguousDomainMap = DomainMap;
    ContiguousColumnMap = ColumnMap;
  }
  else {
    // The domain map isn't linear, so we need a new domain map
    ContiguousDomainMap = rcp(new map_type(DomainMap->getGlobalNumElements(),
                                           DomainMap->getLocalNumElements(), 0, DomainMap->getComm()));
    if(importer) {
      // If there's an importer then we can use it to get a new column map
      go_vector_type MyGIDsHYPRE(DomainMap,ContiguousDomainMap->getLocalElementList());

      // import the HYPRE GIDs
      go_vector_type ColGIDsHYPRE(ColumnMap);
      ColGIDsHYPRE.doImport(MyGIDsHYPRE, *importer, Tpetra::INSERT);
   
      // Make a HYPRE numbering-based column map.
      ContiguousColumnMap = rcp(new map_type(ColumnMap->getGlobalNumElements(),ColGIDsHYPRE.getDataNonConst()(),0, ColumnMap->getComm()));
    }
    else {
      // The problem has matching domain/column maps, and somehow the domain map isn't linear, so just use the new domain map
      ContiguousColumnMap = rcp(new map_type(ColumnMap->getGlobalNumElements(),ContiguousDomainMap->getLocalElementList(), 0, ColumnMap->getComm()));
    }
  }
}

#endif


// =========================================================================
// PETSC Testing
// =========================================================================
#if defined(HAVE_MUELU_PETSC) && defined(HAVE_MUELU_TPETRA) && defined(HAVE_MPI)

/*#define PETSC_CHK_ERR(x)  {                   \
  PetscCall(x); \
  }*/

template<typename Scalar, typename LocalOrdinal, typename GlobalOrdinal, typename Node>
class Petsc_SpmV_Pack {
  typedef Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> crs_matrix_type;
  typedef Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> vector_type;
  typedef Tpetra::Map<LocalOrdinal,GlobalOrdinal,Node> map_type;
public:
  Petsc_SpmV_Pack (const crs_matrix_type& A,
                   const vector_type& X,
                   vector_type& Y) {
    using LO = LocalOrdinal;
    const MPI_Comm & comm = *Teuchos::rcp_dynamic_cast<const Teuchos::MpiComm<int> >(A.getRowMap()->getComm())->getRawMpiComm();
    LO Nx = (LO) X.getMap()->getLocalNumElements();
    LO Ny = (LO) Y.getMap()->getLocalNumElements();

    // NOTE:  Petsc requires contiguous GIDs for the row map
    Teuchos::RCP<const map_type> c_row_map, c_col_map, c_domain_map;
    MakeContiguousMaps(A,c_row_map,c_col_map,c_domain_map);

    // Note: PETSC appears to favor local indices for vector insertion
    std::vector<PetscInt> l_indices(std::max(Nx,Ny));
    for(int i=0; i<std::max(Nx,Ny); i++)
      l_indices[i] = i;

    // x vector
    VecCreate(comm,&x_p);
    VecSetType(x_p,VECMPI);
    VecSetSizes(x_p,Nx, X.getMap()->getGlobalNumElements());
    VecSetValues(x_p,Nx,l_indices.data(),X.getData(0).getRawPtr(),INSERT_VALUES);
    VecAssemblyBegin(x_p);
    VecAssemblyEnd(x_p);

    // y vector
    VecCreate(comm,&y_p);
    VecSetType(y_p,VECMPI);
    VecSetSizes(y_p,Ny, Y.getMap()->getGlobalNumElements());
    VecSetValues(y_p,Ny,l_indices.data(),Y.getData(0).getRawPtr(),INSERT_VALUES);
    VecAssemblyBegin(y_p);
    VecAssemblyEnd(y_p);

    // A matrix
    // NOTE: This is an overallocation, but that's OK
    PetscInt max_nnz = A.getLocalMaxNumRowEntries();
    MatCreateAIJ(comm,c_row_map->getLocalNumElements(),c_domain_map->getLocalNumElements(),PETSC_DECIDE,PETSC_DECIDE,
                 max_nnz, NULL, max_nnz,NULL, &A_p);


    std::vector<PetscInt> new_indices(max_nnz);
    for(LO i = 0; i < (LO)A.getLocalNumRows(); i++){
      typename crs_matrix_type::values_host_view_type     values;
      typename crs_matrix_type::local_inds_host_view_type indices;
      A.getLocalRowView(i, indices, values);
      for(LO j = 0; j < (LO) indices.extent(0); j++){
        new_indices[j] = c_col_map->getGlobalElement(indices(j));
      }
      PetscInt GlobalRow[1];
      PetscInt numEntries = (PetscInt) indices.extent(0);
      GlobalRow[0] = c_row_map->getGlobalElement(i);
      
      MatSetValues(A_p,1,GlobalRow, numEntries, new_indices.data(), values.data(),INSERT_VALUES);
    }
    MatAssemblyBegin(A_p,MAT_FINAL_ASSEMBLY);
    MatAssemblyEnd(A_p,MAT_FINAL_ASSEMBLY);

  }

  ~Petsc_SpmV_Pack() {
    VecDestroy(&x_p);
    VecDestroy(&y_p);
    MatDestroy(&A_p);
  }

  bool spmv(const Scalar alpha, const Scalar beta) {
    int rv = MatMult(A_p,x_p,y_p);
    Kokkos::fence();
    return (rv != 0);
  }

private:
  Mat A_p;
  Vec x_p, y_p;
  
};


#endif



// =========================================================================
// HYPRE Testing
// =========================================================================
#if defined(HAVE_MUELU_HYPRE) && defined(HAVE_MUELU_TPETRA) && defined(HAVE_MPI)

#define HYPRE_CHK_ERR(x)  { \
    if(x!=0) throw std::runtime_error("ERROR: HYPRE returned non-zero exit code"); \
  }

template<typename Scalar, typename LocalOrdinal, typename GlobalOrdinal, typename Node>
class HYPRE_SpmV_Pack {
  typedef Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> crs_matrix_type;
  typedef Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> vector_type;
  typedef Tpetra::Map<LocalOrdinal,GlobalOrdinal,Node> map_type;
public:
  HYPRE_SpmV_Pack (const crs_matrix_type& A,
                   const vector_type& X,
                   vector_type& Y) {
    using LO = LocalOrdinal;
    using GO = GlobalOrdinal;
    
    // Time to copy the matrix / vector over to HYPRE datastructures
    const MPI_Comm & comm = *Teuchos::rcp_dynamic_cast<const Teuchos::MpiComm<int> >(A.getRowMap()->getComm())->getRawMpiComm();


    // NOTE:  Hypre requires contiguous GIDs for the row map
    Teuchos::RCP<const map_type> c_row_map, c_col_map, c_domain_map;
    MakeContiguousMaps(A,c_row_map,c_col_map,c_domain_map);
    
    // Create matrix
    int row_lo = c_row_map->getMinGlobalIndex();
    int row_hi = c_row_map->getMaxGlobalIndex();
    int dom_lo = c_domain_map->getMinGlobalIndex();
    int dom_hi = c_domain_map->getMaxGlobalIndex();
    HYPRE_CHK_ERR(HYPRE_IJMatrixCreate(comm,row_lo,row_hi,dom_lo,dom_hi, &ij_matrix));
    HYPRE_CHK_ERR(HYPRE_IJMatrixSetObjectType(ij_matrix,HYPRE_PARCSR));
    HYPRE_CHK_ERR(HYPRE_IJMatrixInitialize(ij_matrix));


    // Fill matrix
    std::vector<GO> new_indices(A.getLocalMaxNumRowEntries());
    for(LO i = 0; i < (LO)A.getLocalNumRows(); i++){
      typename crs_matrix_type::values_host_view_type     values;
      typename crs_matrix_type::local_inds_host_view_type indices;
      A.getLocalRowView(i, indices, values);
      for(LO j = 0; j < (LO) indices.extent(0); j++){
        new_indices[j] = c_col_map->getGlobalElement(indices(j));
      }
      GO GlobalRow[1];
      GO numEntries = (GO) indices.extent(0);
      GlobalRow[0] = c_row_map->getGlobalElement(i);
      HYPRE_CHK_ERR(HYPRE_IJMatrixSetValues(ij_matrix, 1, &numEntries, GlobalRow, new_indices.data(), values.data()));
    }
     HYPRE_CHK_ERR(HYPRE_IJMatrixAssemble(ij_matrix));
     HYPRE_CHK_ERR(HYPRE_IJMatrixGetObject(ij_matrix, (void**)&parcsr_matrix));

    // Now the x vector
    GO * dom_indices = const_cast<GO*>(c_domain_map->getLocalElementList().getRawPtr());
    HYPRE_CHK_ERR(HYPRE_IJVectorCreate(comm, dom_lo, dom_hi, &x_ij));
    HYPRE_CHK_ERR(HYPRE_IJVectorSetObjectType(x_ij, HYPRE_PARCSR));
    HYPRE_CHK_ERR(HYPRE_IJVectorInitialize(x_ij));
    HYPRE_CHK_ERR(HYPRE_IJVectorSetValues(x_ij,X.getLocalLength(),dom_indices,const_cast<vector_type*>(&X)->getDataNonConst(0).getRawPtr()));
    HYPRE_CHK_ERR(HYPRE_IJVectorAssemble(x_ij));
    HYPRE_CHK_ERR(HYPRE_IJVectorGetObject(x_ij, (void**) &x_par));
    
    
    // Now the y vector
    GO * row_indices = const_cast<GO*>(c_row_map->getLocalElementList().getRawPtr());
    HYPRE_CHK_ERR(HYPRE_IJVectorCreate(comm, row_lo,row_hi, &y_ij));
    HYPRE_CHK_ERR(HYPRE_IJVectorSetObjectType(y_ij, HYPRE_PARCSR));
    HYPRE_CHK_ERR(HYPRE_IJVectorInitialize(y_ij));
    HYPRE_CHK_ERR(HYPRE_IJVectorSetValues(y_ij,Y.getLocalLength(),row_indices,Y.getDataNonConst(0).getRawPtr()));
    HYPRE_CHK_ERR(HYPRE_IJVectorAssemble(y_ij));
    HYPRE_CHK_ERR(HYPRE_IJVectorGetObject(y_ij, (void**) &y_par));
  }

  ~HYPRE_SpmV_Pack() {
    HYPRE_IJMatrixDestroy(ij_matrix);
    HYPRE_IJVectorDestroy(x_ij);
    HYPRE_IJVectorDestroy(y_ij);
  }

  bool spmv(const Scalar alpha, const Scalar beta) {
    int rv = HYPRE_ParCSRMatrixMatvec(alpha,parcsr_matrix, x_par,beta, y_par);
    Kokkos::fence();
    return (rv != 0);
  }

private:
  // NOTE:  The par matrix/vectors are just pointers to the internal data of the ij guys.  They do not need to
  // be deallocated on their own.
  HYPRE_IJMatrix ij_matrix;
  HYPRE_ParCSRMatrix parcsr_matrix;

  HYPRE_IJVector x_ij, y_ij;
  HYPRE_ParVector x_par, y_par;


};

#endif


// =========================================================================
// MAGMASparse Testing
// =========================================================================
#if defined(HAVE_MUELU_MAGMASPARSE) && defined(HAVE_MUELU_TPETRA)
#include <magma_v2.h>
#include <magmasparse.h>
template<typename Scalar, typename LocalOrdinal, typename GlobalOrdinal, typename Node>
class MagmaSparse_SpmV_Pack {
  typedef Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> crs_matrix_type;
  typedef Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> vector_type;

public:
  MagmaSparse_SpmV_Pack (const crs_matrix_type& A,
                         const vector_type& X,
                         vector_type& Y)
  {}

 ~MagmaSparse_SpmV_Pack() {};

  bool spmv(const Scalar alpha, const Scalar beta) { return (true); }
};

template<typename LocalOrdinal, typename GlobalOrdinal>
class MagmaSparse_SpmV_Pack<double,LocalOrdinal,GlobalOrdinal,Tpetra::KokkosCompat::KokkosCudaWrapperNode> {
  // typedefs shared among other TPLs
  typedef double Scalar;
  typedef typename Tpetra::KokkosCompat::KokkosCudaWrapperNode Node;
  typedef Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> crs_matrix_type;
  typedef Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> vector_type;
  typedef typename crs_matrix_type::local_matrix_host_type    KCRS;
  typedef typename KCRS::StaticCrsGraphType              graph_t;
  typedef typename graph_t::row_map_type::non_const_type lno_view_t;
  typedef typename graph_t::row_map_type::const_type     c_lno_view_t;
  typedef typename graph_t::entries_type::non_const_type lno_nnz_view_t;
  typedef typename graph_t::entries_type::const_type     c_lno_nnz_view_t;
  typedef typename KCRS::values_type::non_const_type     scalar_view_t;
  typedef typename Node::device_type device_type;

  typedef typename Kokkos::View<int*,
                                typename lno_nnz_view_t::array_layout,
                                typename lno_nnz_view_t::device_type> int_array_type;

public:
  MagmaSparse_SpmV_Pack (const crs_matrix_type& A,
                         const vector_type& X,
                               vector_type& Y)
  {
    // data access common to other TPLs
    const KCRS & Amat = A.getLocalMatrixHost();
    c_lno_view_t Arowptr = Amat.graph.row_map;
    c_lno_nnz_view_t Acolind = Amat.graph.entries;
    const scalar_view_t Avals = Amat.values;

    // type conversion
    Arowptr_int = int_array_type("Arowptr", Arowptr.extent(0));
    // copy the ordinals into the local view (type conversion)
    copy_view(Arowptr,Arowptr_int);


    m   = static_cast<int>(A.getLocalNumRows());
    n   = static_cast<int>(A.getLocalNumCols());
    nnz = static_cast<int>(Acolind.extent(0));
    vals   = reinterpret_cast<Scalar*>(Avals.data());
    cols   = const_cast<int*>(reinterpret_cast<const int*>(Acolind.data()));
    rowptr = reinterpret_cast<int*>(Arowptr_int.data());

    auto X_lcl = X.template getLocalView<device_type> ();
    auto Y_lcl = Y.template getLocalView<device_type> ();
    x = reinterpret_cast<Scalar*>(X_lcl.data());
    y = reinterpret_cast<Scalar*>(Y_lcl.data());

    magma_init ();
    int device;
    magma_getdevice( &device );
    magma_queue_create( device, &queue );

    // create the magma data container
    magma_dev_Acrs = {Magma_CSR};
    magma_Acrs     = {Magma_CSR};
    magma_dev_x    = {Magma_DENSE};
    magma_dev_y    = {Magma_DENSE};

    //
    magma_dvset_dev (m, 1,         // assume mx1 size
                     x,            // ptr to data on the device
                     &magma_dev_x, // magma vector to populate
                     queue);

    magma_dvset_dev (m, 1,         // assume mx1 size
                     y,            // ptr to data on the device
                     &magma_dev_y, // magma vector to populate
                     queue);
    magma_dcsrset( m, n, rowptr, cols, vals, &magma_Acrs, queue );
    magma_dmtransfer( magma_Acrs, &magma_dev_Acrs, Magma_DEV, Magma_DEV, queue );

  }

  ~MagmaSparse_SpmV_Pack()
  {
    // we dont' own the data... not sure what magma does here...
    magma_dmfree(&magma_dev_x, queue);
    magma_dmfree(&magma_dev_y, queue);
    magma_dmfree(&magma_dev_Acrs, queue);

    magma_finalize ();
  };

  bool spmv(const Scalar alpha, const Scalar beta)
  {
    magma_d_spmv( 1.0, magma_dev_Acrs, magma_dev_x, 0.0, magma_dev_y, queue );
    Kokkos::fence();
  }

private:
  magma_d_matrix magma_dev_Acrs;
  magma_d_matrix magma_Acrs;
  magma_d_matrix magma_dev_x;
  magma_d_matrix magma_dev_y;
  magma_queue_t queue;

  int m = -1;
  int n = -1;
  int nnz = -1;
  Scalar * vals  = nullptr; // aliased
  int * cols     = nullptr; // aliased
  int * rowptr   = nullptr; // copied
  Scalar * x     = nullptr; // aliased
  Scalar * y     = nullptr; // aliased

  // handles to the copied data
  int_array_type Arowptr_int;
};
#endif


// =========================================================================
// CuSparse Testing
// =========================================================================
#if defined(HAVE_MUELU_CUSPARSE) && defined(HAVE_MUELU_TPETRA)

#define CHECK_CUDA(func)                                                       \
{                                                                              \
    cudaError_t status = (func);                                               \
    if (status != cudaSuccess) {                                               \
        printf("CUDA API failed at line %d with error: %s (%d)\n",             \
               __LINE__, cudaGetErrorString(status), status);                  \
    }                                                                          \
}

#define CHECK_CUSPARSE(func)                                                   \
{                                                                              \
    cusparseStatus_t status = (func);                                          \
    if (status != CUSPARSE_STATUS_SUCCESS) {                                   \
        printf("CUSPARSE API failed at line %d with error: %s (%d)\n",         \
               __LINE__, cusparseGetErrorString(status), status);              \
    }                                                                          \
}


template<typename Scalar, typename LocalOrdinal, typename GlobalOrdinal, typename Node>
class CuSparse_SpmV_Pack {
  typedef Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> crs_matrix_type;
  typedef Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> vector_type;

public:
  CuSparse_SpmV_Pack (const crs_matrix_type& A,
                      const vector_type& X,
                            vector_type& Y)
  {}

 ~CuSparse_SpmV_Pack() {};

  cusparseStatus_t spmv(const Scalar alpha, const Scalar beta) { return CUSPARSE_STATUS_SUCCESS; }
};

template<typename LocalOrdinal, typename GlobalOrdinal>
class CuSparse_SpmV_Pack<double,LocalOrdinal,GlobalOrdinal,Tpetra::KokkosCompat::KokkosCudaWrapperNode> {
  // typedefs shared among other TPLs
  typedef double Scalar;
  typedef typename Tpetra::KokkosCompat::KokkosCudaWrapperNode Node;
  typedef Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> crs_matrix_type;
  typedef Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> vector_type;
  typedef typename crs_matrix_type::local_matrix_device_type    KCRS;
  typedef typename KCRS::StaticCrsGraphType              graph_t;
  typedef typename graph_t::row_map_type::non_const_type lno_view_t;
  typedef typename graph_t::row_map_type::const_type     c_lno_view_t;
  typedef typename graph_t::entries_type::non_const_type lno_nnz_view_t;
  typedef typename graph_t::entries_type::const_type     c_lno_nnz_view_t;
  typedef typename KCRS::values_type::non_const_type     scalar_view_t;
  typedef typename Node::device_type device_type;

  typedef typename Kokkos::View<int*,
                                typename lno_nnz_view_t::array_layout,
                                typename lno_nnz_view_t::device_type> cusparse_int_type;
public:
  CuSparse_SpmV_Pack (const crs_matrix_type& A,
                      const vector_type& X,
                            vector_type& Y)
  {
    // data access common to other TPLs
    const KCRS & Amat = A.getLocalMatrixDevice();
    c_lno_view_t Arowptr = Amat.graph.row_map;
    c_lno_nnz_view_t Acolind = Amat.graph.entries;
    const scalar_view_t Avals = Amat.values;


    Arowptr_cusparse = cusparse_int_type("Arowptr", Arowptr.extent(0));
    Acolind_cusparse = cusparse_int_type("Acolind", Acolind.extent(0));
    // copy the ordinals into the local view (type conversion)
    copy_view(Arowptr,Arowptr_cusparse);
    copy_view(Acolind,Acolind_cusparse);


    m   = static_cast<int>(A.getLocalNumRows());
    n   = static_cast<int>(A.getLocalNumCols());
    nnz = static_cast<int>(Acolind_cusparse.extent(0));
    vals   = reinterpret_cast<Scalar*>(Avals.data());
    cols   = reinterpret_cast<int*>(Acolind_cusparse.data());
    rowptr = reinterpret_cast<int*>(Arowptr_cusparse.data());

    auto X_lcl = X.getLocalViewDevice(Tpetra::Access::ReadOnly);
    auto Y_lcl = Y.getLocalViewDevice(Tpetra::Access::ReadWrite);
    x = const_cast<Scalar*>(reinterpret_cast<const Scalar*>(X_lcl.data()));
    y = reinterpret_cast<Scalar*>(Y_lcl.data());

    /* Get handle to the CUBLAS context */
    cublasCreate(&cublasHandle);
    /* Get handle to the CUSPARSE context */
    cusparseCreate(&cusparseHandle);

    CHECK_CUSPARSE(cusparseCreateCsr(&descrA, m, n, nnz,
                                      rowptr, cols, vals,
                                      CUSPARSE_INDEX_32I, CUSPARSE_INDEX_32I,
                                      CUSPARSE_INDEX_BASE_ZERO, CUDA_R_64F));

    CHECK_CUSPARSE(cusparseCreateDnVec(&vecY, m, y, CUDA_R_64F));
    CHECK_CUSPARSE(cusparseCreateDnVec(&vecX, n, x, CUDA_R_64F));
  }

  ~CuSparse_SpmV_Pack () {
    CHECK_CUSPARSE( cusparseDestroySpMat(descrA) );
    CHECK_CUSPARSE( cusparseDestroyDnVec(vecX) );
    CHECK_CUSPARSE( cusparseDestroyDnVec(vecY) );
    cusparseDestroy(cusparseHandle);
    cublasDestroy(cublasHandle);
  }

  cusparseStatus_t spmv(const Scalar alpha, const Scalar beta) {
    // compute: y = alpha*Ax + beta*y

#if CUSPARSE_VERSION >= 11201
    cusparseSpMVAlg_t alg = CUSPARSE_SPMV_ALG_DEFAULT;
#else
    cusparseSpMVAlg_t alg = CUSPARSE_MV_ALG_DEFAULT;
#endif

    size_t bufferSize;
    CHECK_CUSPARSE(cusparseSpMV_bufferSize(cusparseHandle,
                                           transA,
                                           &alpha,
                                           descrA,
                                           vecX,
                                           &beta,
                                           vecY,
                                           CUDA_R_64F,
                                           alg,
                                           &bufferSize));

    void* dBuffer = NULL;
    CHECK_CUDA(cudaMalloc(&dBuffer, bufferSize));

    cusparseStatus_t rc = cusparseSpMV(cusparseHandle,
                                       transA,
                                       &alpha,
                                       descrA,
                                       vecX,
                                       &beta,
                                       vecY,
                                       CUDA_R_64F,
                                       alg,
                                       dBuffer);

    CHECK_CUDA(cudaFree(dBuffer));
    Kokkos::fence();
    return (rc);
  }
private:
  cublasHandle_t     cublasHandle   = 0;
  cusparseHandle_t   cusparseHandle = 0;
  cusparseSpMatDescr_t descrA         = 0;
  cusparseDnVecDescr_t vecX = 0, vecY = 0;

  // CUSPARSE_OPERATION_NON_TRANSPOSE
  // CUSPARSE_OPERATION_TRANSPOSE
  // CUSPARSE_OPERATION_CONJUGATE_TRANSPOSE
  cusparseOperation_t transA = CUSPARSE_OPERATION_NON_TRANSPOSE;
  int m = -1;
  int n = -1;
  int nnz = -1;
  Scalar * vals  = nullptr; // aliased
  int * cols     = nullptr; // copied
  int * rowptr   = nullptr; // copied
  Scalar * x     = nullptr; // aliased
  Scalar * y     = nullptr; // aliased

  // handles to the copied data
  cusparse_int_type Arowptr_cusparse;
  cusparse_int_type Acolind_cusparse;
};
#endif

// =========================================================================
// MKL Testing
// =========================================================================
#if defined(HAVE_MUELU_MKL) && defined(HAVE_MUELU_TPETRA)
#include "mkl.h"


std::string mkl_error(sparse_status_t code) {
  switch(code) {
  case SPARSE_STATUS_SUCCESS:
    return std::string("Success");
  case SPARSE_STATUS_NOT_INITIALIZED:
    return std::string("Empty handle or matrix array");
  case SPARSE_STATUS_ALLOC_FAILED:
    return std::string("Memory allocation failed");
  case SPARSE_STATUS_INVALID_VALUE:
    return std::string("Input contains an invalid value");
  case SPARSE_STATUS_EXECUTION_FAILED:
    return std::string("Execution failed");
  case SPARSE_STATUS_INTERNAL_ERROR:
    return std::string("Internal error");
  case SPARSE_STATUS_NOT_SUPPORTED:
    return std::string("Operation not supported");
  };

}

struct matrix_descr mkl_descr;

void MV_MKL(sparse_matrix_t & AMKL, double * x, double * y) {
  //sparse_status_t mkl_sparse_d_mv (sparse_operation_t operation, double alpha, const sparse_matrix_t A, struct matrix_descr descr, const double *x, double beta, double *y);

  mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE,1.0,AMKL,mkl_descr,x,0.0,y);
  Kokkos::fence();
}

#endif



// =========================================================================
// Tpetra Kernel Testing
// =========================================================================
#if defined(HAVE_MUELU_TPETRA)
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
void MV_Tpetra(const Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> &A,  const Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> &x,   Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> &y) {
  A.apply(x,y);
  Kokkos::fence();
}


template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
void MV_KK(const Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> &A,  const Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> &x,   Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> &y) {
  typedef typename Node::device_type device_type;
  const auto& AK = A.getLocalMatrixDevice();
  auto X_lcl = x.getLocalViewDevice (Tpetra::Access::ReadOnly);
  auto Y_lcl = y.getLocalViewDevice (Tpetra::Access::OverwriteAll);
  KokkosSparse::spmv(KokkosSparse::NoTranspose,Teuchos::ScalarTraits<Scalar>::one(),AK,X_lcl,Teuchos::ScalarTraits<Scalar>::zero(),Y_lcl);
  Kokkos::fence();
}
#endif


// =========================================================================
// =========================================================================
// =========================================================================
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
int main_(Teuchos::CommandLineProcessor &clp, Xpetra::UnderlyingLib& lib, int argc, char *argv[]) {
#include <MueLu_UseShortNames.hpp>

  using Teuchos::RCP; // reference count pointers
  using Teuchos::rcp;
  using Teuchos::TimeMonitor;
  using std::endl;

#if defined(HAVE_MUELU_PETSC) && defined(HAVE_MUELU_TPETRA) && defined(HAVE_MPI)
  PetscInitialize(0,NULL,NULL,NULL);
#endif


  bool success = false;
  bool verbose = true;
  try {
    RCP< const Teuchos::Comm<int> > comm = Teuchos::DefaultComm<int>::getComm();
    int numProc = comm->getSize();

    // =========================================================================
    // Convenient definitions
    // =========================================================================
    //    SC zero = Teuchos::ScalarTraits<SC>::zero(), one = Teuchos::ScalarTraits<SC>::one();

    // Instead of checking each time for rank, create a rank 0 stream
    RCP<Teuchos::FancyOStream> pOut = Teuchos::fancyOStream(Teuchos::rcpFromRef(std::cout));
    Teuchos::FancyOStream& out = *pOut;
    out.setOutputToRootOnly(0);

    // =========================================================================
    // Parameters initialization
    // =========================================================================
    GO nx = 100, ny = 100, nz = 50;
    Galeri::Xpetra::Parameters<GO> galeriParameters(clp, nx, ny, nz, "Laplace2D"); // manage parameters of the test case
    Xpetra::Parameters             xpetraParameters(clp);                          // manage parameters of Xpetra

    bool binaryFormat = false;  clp.setOption("binary", "ascii",  &binaryFormat,   "print timings to screen");
    std::string matrixFile;     clp.setOption("matrixfile",       &matrixFile,     "matrix market file containing matrix");
    std::string rowMapFile;     clp.setOption("rowmap",           &rowMapFile,     "map data file");
    std::string colMapFile;     clp.setOption("colmap",           &colMapFile,     "colmap data file");
    std::string domainMapFile;  clp.setOption("domainmap",        &domainMapFile,  "domainmap data file");
    std::string rangeMapFile;   clp.setOption("rangemap",         &rangeMapFile,   "rangemap data file");

    bool printTimings = true;   clp.setOption("timings", "notimings",  &printTimings, "print timings to screen");
    int  nrepeat      = 1000;   clp.setOption("nrepeat",               &nrepeat,      "repeat the experiment N times");

    bool describeMatrix = true; clp.setOption("showmatrix", "noshowmatrix",  &describeMatrix, "describe matrix");
    bool useStackedTimer = false; clp.setOption("stackedtimer", "nostackedtimer",  &useStackedTimer, "use stacked timer");
    bool verboseModel  = false; clp.setOption("verbosemodel", "noverbosemodel",  &verboseModel, "use stacked verbose performance model");

    // the kernels
    bool do_mkl      = true;
    bool do_tpetra   = true;
    bool do_kk       = true;
    bool do_cusparse = true;
    bool do_magmasparse = true;
    bool do_hypre    = true;
    bool do_petsc    = true;

    bool report_error_norms = false;
    // handle the TPLs
    #if ! defined(HAVE_MUELU_MKL)
      do_mkl = false;
    #endif
    #if ! defined(HAVE_MUELU_CUSPARSE)
      do_cusparse = false;
    #endif
    #if ! defined(HAVE_MUELU_MAGMASPARSE)
      do_magmasparse = false;
    #endif
    #if ! defined(HAVE_MUELU_HYPRE) || !defined(HAVE_MPI)
      do_hypre = false;
    #endif
    #if ! defined(HAVE_MUELU_PETSC) || !defined(HAVE_MPI)
      do_petsc = false;
    #endif


    clp.setOption("mkl",      "nomkl",      &do_mkl,        "Evaluate MKL");
    clp.setOption("tpetra",   "notpetra",   &do_tpetra,     "Evaluate Tpetra");
    clp.setOption("kk",       "nokk",       &do_kk,         "Evaluate KokkosKernels");
    clp.setOption("cusparse", "nocusparse", &do_cusparse,   "Evaluate CuSparse");
    clp.setOption("magamasparse", "nomagmasparse", &do_magmasparse,   "Evaluate MagmaSparse");
    clp.setOption("hypre", "nohypre", &do_hypre,   "Evaluate Hypre");
    clp.setOption("petsc", "nopetsc", &do_petsc,   "Evaluate Petsc");

    clp.setOption("report_error_norms", "noreport_error_norms", &report_error_norms,   "Report L2 norms for the solution");

    std::ostringstream galeriStream;
    std::string rhsFile,coordFile,coordMapFile,nullFile, materialFile; //unused
    typedef typename Teuchos::ScalarTraits<SC>::magnitudeType real_type;
    typedef Xpetra::MultiVector<real_type,LO,GO,NO> RealValuedMultiVector;
    RCP<RealValuedMultiVector> coordinates;
    RCP<MultiVector> nullspace, material, x, b;
    RCP<Matrix> A;
    RCP<const Map> map;

    switch (clp.parse(argc,argv)) {
      case Teuchos::CommandLineProcessor::PARSE_HELP_PRINTED:        return EXIT_SUCCESS;
      case Teuchos::CommandLineProcessor::PARSE_ERROR:
      case Teuchos::CommandLineProcessor::PARSE_UNRECOGNIZED_OPTION: return EXIT_FAILURE;
      case Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL:                               break;
    }

    // Load the matrix off disk (or generate it via Galeri), assuming only one right hand side is loaded.
    MatrixLoad<SC,LO,GO,NO>(comm, lib, binaryFormat, matrixFile, rhsFile, rowMapFile, colMapFile, domainMapFile, rangeMapFile, coordFile, coordMapFile, nullFile, materialFile, map, A, coordinates, nullspace, material, x, b, 1, galeriParameters, xpetraParameters, galeriStream);


    if (do_kk && comm->getSize() > 1) {
      out << "KK was requested, but this kernel this cannot be run on more than one rank. Disabling..." << endl;
      do_kk = false;
    }

    #ifndef HAVE_MUELU_MKL
    if (do_mkl) {
      out << "MKL was requested, but this kernel is not available. Disabling..." << endl;
      do_mkl = false;
    }
    #endif

    #if ! defined(HAVE_MUELU_CUSPARSE)
    if (do_cusparse) {
      out << "CuSparse was requested, but this kernel is not available. Disabling..." << endl;
      do_cusparse = false;
    }
    #else
    if(! std::is_same<NO, Tpetra::KokkosCompat::KokkosCudaWrapperNode>::value) do_cusparse = false;
    #endif

    #if ! defined(HAVE_MUELU_MAGMASPARSE)
    if (do_magmasparse) {
      out << "MagmaSparse was requested, but this kernel is not available. Disabling..." << endl;
      do_magmasparse = false;
    }
    #endif

    #if ! defined(HAVE_MUELU_HYPRE)
    if (do_hypre) {
      out << "Hypre was requested, but this kernel is not available. Disabling..." << endl;
      do_hypre = false;
    }
    #endif

    #if ! defined(HAVE_MUELU_PETSC)
    if (do_hypre) {
      out << "Petsc was requested, but this kernel is not available. Disabling..." << endl;
      do_petsc = false;
    }
    #endif

    // simple hack to randomize order of experiments
    enum class Experiments { MKL=0, TPETRA, KK, CUSPARSE, MAGMASPARSE, HYPRE, PETSC };
    const char * const experiment_id_to_string[] = {
        "MKL        ",
        "Tpetra     ",
        "KK         ",
        "CuSparse   ",
        "MagmaSparse",
        "HYPRE",
        "PETSC"};
      
    std::vector<Experiments> my_experiments;
    // add the experiments we will run

    #ifdef HAVE_MUELU_MKL
    if (do_mkl) my_experiments.push_back(Experiments::MKL);   // MKL
    #endif

    #ifdef HAVE_MUELU_CUSPARSE
    if (do_cusparse) my_experiments.push_back(Experiments::CUSPARSE);   // CuSparse
    #endif

    #ifdef HAVE_MUELU_MAGMASPARSE
    if (do_magmasparse) my_experiments.push_back(Experiments::MAGMASPARSE);   // MagmaSparse
    #endif

    #if defined(HAVE_MUELU_HYPRE) && defined(HAVE_MPI)
    if (do_hypre) my_experiments.push_back(Experiments::HYPRE);   // HYPRE
    #endif

    #if defined(HAVE_MUELU_PETSC) && defined(HAVE_MPI)
    if (do_petsc) my_experiments.push_back(Experiments::PETSC);   // PETSc
    #endif

    // assume these are available
    if (do_tpetra)  my_experiments.push_back(Experiments::TPETRA);     // Tpetra
    if (do_kk)     my_experiments.push_back(Experiments::KK);     // KK


    out << "========================================================" << endl
        << xpetraParameters
        // << matrixParameters
        << "========================================================" << endl
        << "Template Types:" << endl
        << "  Scalar:        " << Teuchos::demangleName(typeid(SC).name()) << endl
        << "  LocalOrdinal:  " << Teuchos::demangleName(typeid(LO).name()) << endl
        << "  GlobalOrdinal: " << Teuchos::demangleName(typeid(GO).name()) << endl
        << "  Node:          " << Teuchos::demangleName(typeid(NO).name()) << endl
        << "Sizes:" << endl
        << "  Scalar:        " << sizeof(SC) << endl
        << "  LocalOrdinal:  " << sizeof(LO) << endl
        << "  GlobalOrdinal: " << sizeof(GO) << endl
        << "========================================================" << endl
        << "Matrix:        " << Teuchos::demangleName(typeid(Matrix).name()) << endl
        << "Vector:        " << Teuchos::demangleName(typeid(MultiVector).name()) << endl
        << "Hierarchy:     " << Teuchos::demangleName(typeid(Hierarchy).name()) << endl
        << "========================================================" << endl
        << " MPI Ranks:    " << numProc <<endl
        << "========================================================" << endl;

#if defined(HAVE_MUELU_TPETRA) && defined(HAVE_TPETRA_INST_OPENMP)
    out<< "Tpetra::KokkosCompat::KokkosOpenMPWrapperNode::execution_space().concurrency() = "<<Tpetra::KokkosCompat::KokkosOpenMPWrapperNode::execution_space().concurrency()<<endl
       << "========================================================" << endl;
#endif

    // =========================================================================
    // Problem construction
    // =========================================================================
    if (useStackedTimer)
      stacked_timer = rcp(new Teuchos::StackedTimer("MueLu_MatvecKernelDriver"));
    else
      globalTimeMonitor = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("MatrixRead: S - Global Time")));

    comm->barrier();


    RCP<MultiVector> y = Xpetra::VectorFactory<SC,LO,GO,Node>::Build(A->getRowMap());
    RCP<MultiVector> y_baseline = Xpetra::VectorFactory<SC,LO,GO,Node>::Build(A->getRowMap());
    x->putScalar(Teuchos::ScalarTraits<Scalar>::one());
    y->putScalar(Teuchos::ScalarTraits<Scalar>::nan());
    y_baseline->putScalar(Teuchos::ScalarTraits<Scalar>::nan());


#ifdef HAVE_MUELU_TPETRA
    typedef Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> crs_matrix_type;
    typedef Tpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> vector_type;
    RCP<const crs_matrix_type> At;
    vector_type xt;
    vector_type yt;

    At = Utilities::Op2TpetraCrs(A);
    const crs_matrix_type &Att = *At;
    xt = Xpetra::toTpetra(*x);
    yt = Xpetra::toTpetra(*y);

    size_t l_permutes = 0, g_permutes = 0;
    if(!At->getGraph()->getImporter().is_null()) {
      l_permutes = At->getGraph()->getImporter()->getNumPermuteIDs();
      Teuchos::reduceAll(*comm, Teuchos::REDUCE_SUM,1,&l_permutes,&g_permutes);
    }

  #if defined(HAVE_MUELU_CUSPARSE)
    typedef CuSparse_SpmV_Pack<SC,LO,GO,Node> CuSparse_thing_t;
    CuSparse_thing_t cusparse_spmv(*At, xt, yt);
  #endif

  #if defined(HAVE_MUELU_MAGMASPARSE)
    typedef MagmaSparse_SpmV_Pack<SC,LO,GO,Node> MagmaSparse_thing_t;
    MagmaSparse_thing_t magmasparse_spmv(*At, xt, yt);
  #endif
  #if defined(HAVE_MUELU_HYPRE) && defined(HAVE_MPI)
    typedef HYPRE_SpmV_Pack<SC,LO,GO,Node> HYPRE_thing_t;
    HYPRE_thing_t hypre_spmv(*At, xt, yt);
  #endif
  #if defined(HAVE_MUELU_PETSC) && defined(HAVE_MPI)
    typedef Petsc_SpmV_Pack<SC,LO,GO,Node> Petsc_thing_t;
    Petsc_thing_t petsc_spmv(*At, xt, yt);
  #endif


  #if defined(HAVE_MUELU_MKL)
    // typedefs shared among other TPLs
    typedef typename crs_matrix_type::local_matrix_host_type    KCRS;
    typedef typename KCRS::StaticCrsGraphType              graph_t;
    typedef typename graph_t::row_map_type::non_const_type lno_view_t;
    typedef typename graph_t::row_map_type::const_type     c_lno_view_t;
    typedef typename graph_t::entries_type::non_const_type lno_nnz_view_t;
    typedef typename graph_t::entries_type::const_type     c_lno_nnz_view_t;
    typedef typename KCRS::values_type::non_const_type     scalar_view_t;
    typedef typename Node::device_type device_type;

    // data access common to other TPLs
    const KCRS & Amat = At->getLocalMatrixHost();
    c_lno_view_t Arowptr = Amat.graph.row_map;
    c_lno_nnz_view_t Acolind = Amat.graph.entries;
    const scalar_view_t Avals = Amat.values;

    // MKL specific things
    sparse_matrix_t mkl_A;
    typedef typename Kokkos::View<MKL_INT*,typename lno_nnz_view_t::array_layout,typename lno_nnz_view_t::device_type> mkl_int_type;

    mkl_int_type ArowptrMKL("Arowptr", Arowptr.extent(0));
    mkl_int_type AcolindMKL("Acolind", Acolind.extent(0));
    copy_view(Arowptr,ArowptrMKL);
    copy_view(Acolind,AcolindMKL);
    double * mkl_xdouble = nullptr;
    double * mkl_ydouble = nullptr;
    mkl_descr.type = SPARSE_MATRIX_TYPE_GENERAL;

    if(std::is_same<Scalar,double>::value) {
      mkl_sparse_d_create_csr(&mkl_A,
                              SPARSE_INDEX_BASE_ZERO,
                              At->getLocalNumRows(),
                              At->getLocalNumCols(),
                              ArowptrMKL.data(),
                              ArowptrMKL.data()+1,
                              AcolindMKL.data(),
                              (double*)Avals.data());
    }
    else
      throw std::runtime_error("MKL Type Mismatch");

  #endif // end MKL
#endif


    //    globalTimeMonitor = Teuchos::null;
    comm->barrier();

    out << "Matrix Read complete." << endl;
    if (describeMatrix) {
        out << "Matrix A:" << endl
            << *A
            << "========================================================" << endl;
    }
    // save std::cout formating
    std::ios cout_default_fmt_flags(NULL);
    cout_default_fmt_flags.copyfmt(std::cout);

    // random source
    std::random_device rd;
    std::mt19937 random_source (rd());

    #ifdef HAVE_MUELU_TPETRA
    // compute the baseline
    vector_type yt_baseline = Xpetra::toTpetra(*y_baseline);
    if (report_error_norms) MV_Tpetra(*At,xt,yt_baseline);
    const bool error_check_y = true;
    #else
    const bool error_check_y = false;
    #endif
    std::vector<typename Teuchos::ScalarTraits< Scalar >::magnitudeType> dummy;
    dummy.resize(1);
    Teuchos::ArrayView< typename Teuchos::ScalarTraits< Scalar >::magnitudeType > y_norms (dummy.data(), 1);

    // no need for a barrier, because the randomization process uses a collective.
    if ( !my_experiments.empty() ) {


    for (int i=0; i<nrepeat; i++) {

      // randomize the experiments
      if (comm->getRank() == 0 ) {
        std::shuffle(my_experiments.begin(), my_experiments.end(), random_source);
      }

      // Broadcast this ordering to the other processes
      comm->broadcast(0,
                      static_cast<int>(sizeof(Experiments::TPETRA)*my_experiments.size()),
                      reinterpret_cast<char *>(my_experiments.data()));

      // loop over the randomized experiments
      for (const auto& experiment_id : my_experiments) {
        switch (experiment_id) {

        #ifdef HAVE_MUELU_MKL
        // MKL
        case Experiments::MKL:
        {
            TimeMonitor t(*TimeMonitor::getNewTimer("MV MKL: Total"));
            auto X_lcl = xt.getLocalViewDevice(Tpetra::Access::ReadOnly);
            auto Y_lcl = yt.getLocalViewDevice(Tpetra::Access::OverwriteAll);
            mkl_xdouble = (double*)X_lcl.data();
            mkl_ydouble = (double*)Y_lcl.data();
            MV_MKL(mkl_A,mkl_xdouble,mkl_ydouble);
        }
          break;
        #endif

        #ifdef HAVE_MUELU_TPETRA
        // KK Algorithms
        case Experiments::KK:
        {
           TimeMonitor t(*TimeMonitor::getNewTimer("MV KK: Total"));
           MV_KK(Att,xt,yt);
        }
          break;
        // Tpetra
        case Experiments::TPETRA:
        {
           TimeMonitor t(*TimeMonitor::getNewTimer("MV Tpetra: Total"));
           MV_Tpetra(*At,xt,yt);
        }
          break;
        #endif

        #ifdef HAVE_MUELU_CUSPARSE
        // CUSPARSE
        case Experiments::CUSPARSE:
        {
           const Scalar alpha = 1.0;
           const Scalar beta = 0.0;
           TimeMonitor t(*TimeMonitor::getNewTimer("MV CuSparse: Total"));
           cusparse_spmv.spmv(alpha,beta);
        }
          break;
        #endif

        #ifdef HAVE_MUELU_MAGMASPARSE
        // Magma CSR
        case Experiments::MAGMASPARSE:
        {
           const Scalar alpha = 1.0;
           const Scalar beta = 0.0;
           TimeMonitor t(*TimeMonitor::getNewTimer("MV MagmaSparse: Total"));
           magmasparse_spmv.spmv(alpha,beta);
        }
          break;
        #endif

        #ifdef HAVE_MUELU_HYPRE
        // HYPRE
        case Experiments::HYPRE:
        {
           const Scalar alpha = 1.0;
           const Scalar beta = 0.0;
           TimeMonitor t(*TimeMonitor::getNewTimer("MV HYPRE: Total"));
           hypre_spmv.spmv(alpha,beta);
        }
          break;
        #endif

        #ifdef HAVE_MUELU_PETSC
        // PETSc
        case Experiments::PETSC:
        {
           const Scalar alpha = 1.0;
           const Scalar beta = 0.0;
           TimeMonitor t(*TimeMonitor::getNewTimer("MV Petsc: Total"));
           petsc_spmv.spmv(alpha,beta);
        }
          break;
        #endif

        default:
          std::cerr << "Unknown experiment ID encountered: " << (int) experiment_id << std::endl;
        }
        //TODO: add a correctness check
        // For now, all things alias x/y, so we can test yt (flakey and scary, but you only live once)
        if (error_check_y && report_error_norms) {
          // compute ||y||_2
          y_norms[0] = -1;
          y->norm2(y_norms);
          const auto y_norm2 = y_norms[0];

          y_norms[0] = -1;
          yt.norm2(y_norms);
          const auto y_mv_norm2 = y_norms[0];

          y->update(-Teuchos::ScalarTraits<Scalar>::one(), *y_baseline, Teuchos::ScalarTraits<Scalar>::one());

          y_norms[0] = -1;
          y->norm2(y_norms);
          const auto y_err =  y_norms[0];

          y->putScalar(Teuchos::ScalarTraits<Scalar>::nan());;

          y_norms[0] = -1;
          y_baseline->norm2(y_norms);
          const auto y_baseline_norm2 = y_norms[0];

          y_norms[0] = -1;
          yt.norm2(y_norms);
          const auto y_mv_norm2_next_itr = y_norms[0];

          std::cout << "ExperimentID: " << experiment_id_to_string[(int) experiment_id] << ", ||y-y_hat||_2 = "
                    << std::setprecision(std::numeric_limits<Scalar>::digits10 + 1)
                    << std::scientific << y_err
                    << ", ||y||_2 = " << y_norm2
                    << ", ||y_baseline||_2 = " << y_baseline_norm2
                    << ", ||y_ptr|| == ||y_mv||:  " << std::boolalpha << (y_mv_norm2 == y_norm2)
                    << ", setting y to nan ... ||y||_2 for next iter: " << y_mv_norm2_next_itr
                    << "\n";
        }
        
        // We need to both fence and barrier to make sure the kernels do not overlap
        Kokkos::fence();
        comm->barrier();
      }// end random exp loop
    } // end repeat
    } // end ! my_experiments.empty()
    // restore the IO stream
    std::cout.copyfmt(cout_default_fmt_flags);


    if (useStackedTimer) {
      stacked_timer->stop("MueLu_MatvecKernelDriver");
      Teuchos::StackedTimer::OutputOptions options;
      options.output_fraction = options.output_histogram = options.output_minmax = true;
      stacked_timer->report(out, comm, options);
    } else {
      TimeMonitor::summarize(A->getRowMap()->getComm().ptr(), std::cout, false, true, false, Teuchos::Union, "", true);
    }

#if defined(HAVE_MUELU_MKL)
    mkl_sparse_destroy(mkl_A);
#endif

    // ==========================================
    // Performance Models
    // ==========================================
    report_performance_models<Matrix>(A,nrepeat,verboseModel);
    globalTimeMonitor = Teuchos::null;

    success = true;
  }
  TEUCHOS_STANDARD_CATCH_STATEMENTS(verbose, std::cerr, success);

#if defined(HAVE_MUELU_PETSC) && defined(HAVE_MUELU_TPETRA) && defined(HAVE_MPI)
  PetscFinalize();
#endif


  return ( success ? EXIT_SUCCESS : EXIT_FAILURE );
} //main




//- -- --------------------------------------------------------
#define MUELU_AUTOMATIC_TEST_ETI_NAME main_
#include "MueLu_Test_ETI.hpp"

int main(int argc, char *argv[]) {
  return Automatic_Test_ETI(argc,argv);
}

