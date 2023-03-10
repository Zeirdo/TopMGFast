//Copyright (c) 2014 - 2019, The Trustees of Indiana University.
//
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS,
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//limitations under the License.

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

//#if defined (_WIN32) || defined (_WIN64) || defined (__MINGW32__) || defined (__MINGW64__)
//#include "common/thread/simple_thread_pool.hpp"
//#else
//#include <sys/wait.h>
//#endif

#include "common/thread/simple_thread_pool.hpp"

#include "common/util/file_util.hpp"
#include "common/base/mod_util.hpp"
#include "seq/fasta_sub_util.hpp"
#include "ms/spec/msalign_util.hpp"
#include "prsm/prsm_xml_writer.hpp"
#include "prsm/prsm_reader.hpp"
#include "prsm/prsm_str_merge.hpp"
#include "prsm/simple_prsm_reader.hpp"
#include "prsm/simple_prsm_util.hpp"
#include "prsm/simple_prsm_xml_writer.hpp"
#include "prsm/simple_prsm_xml_writer_util.hpp"
#include "search/graph/proteo_graph_reader.hpp"
#include "search/graph/spec_graph_reader.hpp"
#include "search/graphalign/graph_align.hpp"
#include "search/graphalign/graph_align_processor.hpp"



namespace toppic {


std::function<void()> geneTask(GraphAlignMngPtr mng_ptr,
                               ModPtrVec var_mod_ptr_vec,
                               int spectrum_num, int idx) {
  return [mng_ptr, var_mod_ptr_vec, spectrum_num, idx]() {
    PrsmParaPtr prsm_para_ptr = mng_ptr->prsm_para_ptr_;
    SpParaPtr sp_para_ptr = prsm_para_ptr->getSpParaPtr();
    std::string db_file_name = prsm_para_ptr->getSearchDbFileName();
    std::string sp_file_name = prsm_para_ptr->getSpectrumFileName();
    //std::string diagonalInfo_file_name = mng_ptr->diag_info_file_name_;

    std::string input_file_name = file_util::basename(sp_file_name) + "." + mng_ptr->input_file_ext_ + "_" + str_util::toString(idx);

    //std::cout << "geneTask input_file_name: " << sp_file_name << "." << mng_ptr->input_file_ext_ << "_" << str_util::toString(idx) << std::endl;
    FastaIndexReaderPtr reader_ptr = std::make_shared<FastaIndexReader>(db_file_name);
    //std::cout << "geneTask db_file_name: " << db_file_name << std::endl;
    //std::cout << "geneTask sp_file_name: " << sp_file_name << std::endl;
    //std::cout << "string input file name: " << input_file_name << std::endl;
    SimplePrsmReader simple_prsm_reader(input_file_name);
    SimplePrsmStrPtr prsm_ptr = simple_prsm_reader.readOnePrsmStr();
    int group_spec_num = prsm_para_ptr->getGroupSpecNum();
    MsAlignReader sp_reader(sp_file_name, group_spec_num,
                            sp_para_ptr->getActivationPtr(),
                            sp_para_ptr->getSkipList());

    SpecGraphReader spec_reader(sp_file_name,
                                prsm_para_ptr->getGroupSpecNum(),
                                mng_ptr->convert_ratio_,
                                sp_para_ptr);

    //PrsmXmlWriterPtr writer_ptr
    //    = std::make_shared<PrsmXmlWriter>(file_util::basename(sp_file_name) + "." + mng_ptr->output_file_ext_ + "_" + str_util::toString(idx));
    //std::cout << "geneTask writer_ptr: " << sp_file_name << "." << mng_ptr->output_file_ext_ << "_" << str_util::toString(idx) << std::endl;
    SpectrumSetPtr spec_set_ptr = sp_reader.getNextSpectrumSet(sp_para_ptr)[0];
    ProteoAnnoPtr proteo_anno_ptr
        = std::make_shared<ProteoAnno>(prsm_para_ptr->getFixModPtrVec(),
                                       prsm_para_ptr->getProtModPtrVec(),
                                       var_mod_ptr_vec);
                                       
    
    int cnt = 0;
    while (spec_set_ptr != nullptr) {
      cnt += group_spec_num;
      if (spec_set_ptr->isValid()) {      
        int spec_id = spec_set_ptr->getSpectrumId();
        std::vector<SimplePrsmStrPtr> selected_prsm_ptrs;
        while (prsm_ptr != nullptr && prsm_ptr->getSpectrumId() == spec_id) {
          selected_prsm_ptrs.push_back(prsm_ptr);
          prsm_ptr = simple_prsm_reader.readOnePrsmStr();
        }
        //std::cout << "whileNum is : " << whileNum << std::endl;

        if(spec_id >= 3801){            
          if (selected_prsm_ptrs.size() > 0) {
            SpecGraphPtrVec spec_ptr_vec
                = spec_reader.getNextSpecGraphPtrVec(spec_set_ptr, mng_ptr->prec_error_);
      
            //std::cout << "selected_prsm_ptrs: " << selected_prsm_ptrs.size() << std::endl;
            for (size_t i = 0; i < selected_prsm_ptrs.size(); i++) {
              std::string seq_name = selected_prsm_ptrs[i]->getSeqName();
              std::string seq_desc = selected_prsm_ptrs[i]->getSeqDesc();
              //std::vector<FastaSubSeqPtr> seq_ptr_vec 
              //= reader_ptr->readFastaSubSeqVec(seq_name, seq_desc);
              FastaSeqPtr seq_ptr = reader_ptr->readFastaSeq(seq_name, seq_desc);
              std::vector<FastaSubSeqPtr> seq_ptr_vec = fasta_sub_util::breakSeq(seq_ptr);
              std::cout << "seq_name: " << seq_name << std::endl;
              for (size_t j = 0; j < seq_ptr_vec.size(); j++) {
                for (size_t k = 0; k < spec_ptr_vec.size(); k++) {
                  proteo_anno_ptr->anno(seq_ptr_vec[j]->getRawSeq(), seq_ptr_vec[j]->isNTerm());
                  MassGraphPtr graph_ptr = getMassGraphPtr(proteo_anno_ptr, mng_ptr->convert_ratio_);
                  //createHash(graph_ptr,seq_name);
                  ProteoGraphPtr proteo_ptr = std::make_shared<ProteoGraph>(seq_ptr_vec[j],
                                                                            prsm_para_ptr->getFixModPtrVec(),
                                                                            graph_ptr,
                                                                            proteo_anno_ptr->isNme(),
                                                                            mng_ptr->convert_ratio_,
                                                                            mng_ptr->max_known_mods_,
                                                                            mng_ptr->getIntMaxPtmSumMass(),
                                                                            mng_ptr->proteo_graph_gap_,
                                                                            mng_ptr->var_ptm_in_gap_);
                  GraphAlignPtr graph_align
                      = std::make_shared<GraphAlign>(mng_ptr, proteo_ptr, spec_ptr_vec[k]);
                  //graph_align->process_v2();
                  //graph_align->diagonal_v2(813, 0);
                  //graph_align->diagonal_v3(813, 0);
                  graph_align->TopMGRefine();
                  //graph_align->oriProcess();


                  //for (int shift = 0; shift <= mng_ptr->n_unknown_shift_; shift++) {
                  //  PrsmPtr prsm_ptr = graph_align->geneResult(shift);
                  //  if (prsm_ptr != nullptr) {
                  //    writer_ptr->write(prsm_ptr);
                  //  }
                  //}
                  graph_align = nullptr;
                }
              }                         
            }          
          }
        
        
        }
      
      }
      if (idx == 0) {
        std::cout << std::flush << "Mass graph alignment - processing " << cnt
            << " of " << spectrum_num << " spectra.\r";
      }
      spec_set_ptr = sp_reader.getNextSpectrumSet(sp_para_ptr)[0];
    } // end while;



  };
}

void SimplePrsmFilter(SimplePrsmPtrVec & selected_prsm_ptrs) {
  if (selected_prsm_ptrs.size() == 0) return;
  if (selected_prsm_ptrs.size() == 1) {
    if (selected_prsm_ptrs[0]->getScore() < 10) selected_prsm_ptrs.clear();
    return;
  }
  std::sort(selected_prsm_ptrs.begin(), selected_prsm_ptrs.end(), SimplePrsm::cmpScoreDec);
  selected_prsm_ptrs.erase(std::remove_if(selected_prsm_ptrs.begin(), selected_prsm_ptrs.end(),
                                          [] (const SimplePrsmPtr & p) {return p->getScore() < 10;}),
                           selected_prsm_ptrs.end());
  selected_prsm_ptrs.erase(std::unique(selected_prsm_ptrs.begin(), selected_prsm_ptrs.end(),
                                       [] (const SimplePrsmPtr & a, const SimplePrsmPtr & b) {
                                         return a->getSpectrumScan() == b->getSpectrumScan()
                                           && a->getSeqName() == b->getSeqName();
                                       }),
                           selected_prsm_ptrs.end());
}

void GraphAlignProcessor::process() {
  PrsmParaPtr prsm_para_ptr = mng_ptr_->prsm_para_ptr_;
  SpParaPtr sp_para_ptr = prsm_para_ptr->getSpParaPtr();
  sp_para_ptr->prec_error_ = 0;
  std::string db_file_name = prsm_para_ptr->getSearchDbFileName();
  LOG_DEBUG("Search db file name " << db_file_name);
  std::string sp_file_name = prsm_para_ptr->getSpectrumFileName();
  std::string var_mod_file_name = mng_ptr_->var_mod_file_name_;
  LOG_DEBUG("start reading " << var_mod_file_name);
  ModPtrVec var_mod_ptr_vec = mod_util::readModTxt(var_mod_file_name)[2];
  LOG_DEBUG("end reading " << var_mod_file_name);

  int spectrum_num = msalign_util::getSpNum(prsm_para_ptr->getSpectrumFileName());


//#if defined (_WIN32) || defined (_WIN64) || defined (__MINGW32__) || defined (__MINGW64__)
  std::vector<ThreadPtr> thread_vec;
  //std::cout << "thread_num_ : " << mng_ptr_->thread_num_ << std::endl;
  for (int i = 0; i < mng_ptr_->thread_num_; i++) {
    ThreadPtr thread_ptr = std::make_shared<boost::thread>(geneTask(mng_ptr_, var_mod_ptr_vec, spectrum_num, i));
    thread_vec.push_back(thread_ptr);
  }

  for (size_t i = 0; i < thread_vec.size(); i++) {
    if (thread_vec[i]->joinable()) thread_vec[i]->join();
  }

  time_t now = time(0);
  tm *ltm = localtime(&now);
  std::cout << "The ending time of the program: " << ltm->tm_hour << ":" << ltm->tm_min << ":" << ltm->tm_sec << std::endl;

  exit(0);

/*
#else
  int n = mng_ptr_->thread_num_;

  pid_t pids;

  for (int i = 0; i < n; i++) {
    pids = fork();
    if (pids < 0) {
      std::abort();
    } else if (pids == 0) {
      auto task = geneTask(reader_ptr, mng_ptr_, var_mod_ptr_vec, spectrum_num, i);
      task();
      exit(0);
    }
  }

  int status;
  while (n > 0) {
    wait(&status);
    --n;
  }
#endif
*/
  std::cout << std::flush << "Mass graph alignment - processing " << spectrum_num
      << " of " << spectrum_num << " spectra." << std::endl;

  // combine result files
  std::vector<std::string> input_exts;
  for (int i = 0; i < mng_ptr_->thread_num_; i++) {
    std::string fname = mng_ptr_->output_file_ext_ + "_" + str_util::toString(i);
    input_exts.push_back(fname);
  }

  int top_num = (mng_ptr_->n_unknown_shift_ + 1) * 4;
  PrsmStrMergePtr merge_ptr
      = std::make_shared<PrsmStrMerge>(sp_file_name, input_exts,
                                       mng_ptr_->output_file_ext_, top_num);
  bool normalization = true;
  merge_ptr->process(normalization);
  merge_ptr = nullptr;

  // remove temporary files
  
  for (int t = 0; t < mng_ptr_->thread_num_; t++) {
    file_util::cleanTempFiles(sp_file_name, mng_ptr_->input_file_ext_ + "_" + str_util::toString(t));
    file_util::cleanTempFiles(sp_file_name, mng_ptr_->output_file_ext_ + "_" + str_util::toString(t));
  }
  
}


}  // namespace toppic

