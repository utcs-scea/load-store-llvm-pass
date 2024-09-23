use llvm_plugin::inkwell::module::Module;
use llvm_plugin::inkwell::types::AnyTypeEnum;
use llvm_plugin::inkwell::values::{BasicMetadataValueEnum, BasicValueEnum, FunctionValue, InstructionOpcode, InstructionValue, PointerValue};
use llvm_plugin::inkwell::basic_block::BasicBlock;
use llvm_plugin::inkwell::builder::Builder;
use llvm_plugin::{
    LlvmModulePass, ModuleAnalysisManager, PassBuilder, PipelineParsing, PreservedAnalyses
};
use either::Either;

#[llvm_plugin::plugin(name = "scea-passes", version = "0.1")]
fn plugin_registrar(builder: &mut PassBuilder) {
  builder.add_module_pipeline_parsing_callback(|name, manager| {
    match name {
      "load-store-pass" => {
        manager.add_pass(LoadStorePass);
        //manager.add_pass(GlobalVariablePointerRenamePass);
        PipelineParsing::Parsed
      },
      "globalify-consts-pass" => {
        manager.add_pass(GlobalifyConstsPass);
        PipelineParsing::Parsed
      },
      _ => PipelineParsing::NotParsed,
    }
  });
}

struct LoadStorePass;
impl LlvmModulePass for LoadStorePass {
  fn run_pass(
    &self,
    module: &mut Module,
    _manager: &ModuleAnalysisManager
  ) -> PreservedAnalyses {
    let mut one_load_or_store = false;
    // transform the IR
    let globalify_func = module.get_function("globalify").unwrap();
    let loadsi64_func = module.get_function("__pando__replace_load64").unwrap();
    let loadsptr_func = module.get_function("__pando__replace_loadptr").unwrap();
    let storei64_func = module.get_function("__pando__replace_store64").unwrap();
    let storeptr_func = module.get_function("__pando__replace_storeptr").unwrap();
    let cx = module.get_context();
    let builder = cx.create_builder();
    let fs = module.get_functions();
    for f in fs {
      match f.get_name().to_str().unwrap() {
        "__pando__replace_load64" => {continue;}
        "__pando__replace_loadptr" => {continue;}
        "__pando__replace_store64" => {continue;}
        "__pando__replace_storeptr" => {continue;}
        "check_if_global" => {continue;}
        "deglobalify" => {continue;}
        "globalify" => {continue;}
        _ => {}
      }
      for b in f.get_basic_block_iter() {
        for instr in b.get_instructions() {
          match instr.get_opcode() {
            InstructionOpcode::Load  => {
              one_load_or_store = true;
              builder.position_at(b, &instr);
              let operand = instr.get_operand(0).unwrap().left().unwrap();
              match operand {
                BasicValueEnum::PointerValue(ptr_val) => {
                  let func = match instr.get_type() {
                    AnyTypeEnum::PointerType(_) => {loadsptr_func}
                    AnyTypeEnum::IntType(_) => {loadsi64_func}
                    _ => {loadsi64_func}
                  };
                  let replace_instr: InstructionValue = match builder
                    .build_direct_call(func, &[BasicMetadataValueEnum::PointerValue(ptr_val)], "loads_func")
                    .unwrap()
                    .try_as_basic_value() {
                      Either::Left(basic_value) => {
                        if basic_value.is_pointer_value() {
                          basic_value.into_pointer_value().as_instruction().unwrap()
                        } else if basic_value.is_int_value() {
                          basic_value.into_int_value().as_instruction().unwrap()
                        } else {
                          panic!("This is unreachable for the call type")
                        }
                      }
                      Either::Right(instr_value) => {instr_value}
                  };
                  instr.replace_all_uses_with(&replace_instr);
                  instr.erase_from_basic_block();
                }
                _ => { println!("{:#?}", operand); panic!("Should not happen");}
              }
            }
            InstructionOpcode::Store  => {
              one_load_or_store = true;
              builder.position_at(b, &instr);
              let operand0 = instr.get_operand(0).unwrap().left().unwrap();
              let operand1 = instr.get_operand(1).unwrap().left().unwrap();
              let func = match operand0 {
                BasicValueEnum::PointerValue(_) => {storeptr_func}
                BasicValueEnum::IntValue(_) => {storei64_func}
                _ => {panic!("Unreachable {:#?}", operand0)}
              };
              let replace_instr: InstructionValue = match builder
                .build_direct_call(func,
                  &[BasicMetadataValueEnum::try_from(operand0).unwrap(), BasicMetadataValueEnum::try_from(operand1).unwrap()],
                  "stores_func")
                .unwrap()
                .try_as_basic_value() {
                  Either::Left(basic_value) => {
                    if basic_value.is_pointer_value() {
                      basic_value.into_pointer_value().as_instruction().unwrap()
                    } else if basic_value.is_int_value() {
                      basic_value.into_int_value().as_instruction().unwrap()
                    } else {
                      panic!("This is unreachable for the call type")
                    }
                  }
                  Either::Right(instr_value) => {instr_value}
              };
              instr.replace_all_uses_with(&replace_instr);
              instr.erase_from_basic_block();
            }
            InstructionOpcode::Alloca => {
              one_load_or_store = true;
              builder.position_at(b, &instr.get_next_instruction().unwrap());
              let ptr_val = PointerValue::try_from(instr).unwrap();
              let next_instr: InstructionValue = match builder
                .build_direct_call(globalify_func, &[BasicMetadataValueEnum::PointerValue(ptr_val)], "globalized")
                .unwrap()
                .try_as_basic_value() {
                  Either::Left(basic_value) => {basic_value.into_pointer_value().as_instruction().unwrap()}
                  Either::Right(instr_value) => {instr_value}
              };
              instr.replace_all_uses_with(&next_instr);
              next_instr.set_operand(0, ptr_val);
            }
            _ => {}
          }
        }
      }
    }
    one_load_or_store
      .then_some(PreservedAnalyses::None)
      .unwrap_or(PreservedAnalyses::All)
  }
}


///////////////////////////////////////////////////////////////////////////////


// struct GlobalVariablePointerRenamePass;

// impl GlobalVariablePointerRenamePass {
//   fn process_instr<'a>(builder: &'a Builder<'a>, func: FunctionValue<'a>, instr: InstructionValue<'a>) -> (Vec<InstructionValue<'a>>, Vec<BasicBlock<'a>>, bool) {
//     let mut one_global_variable = false;
//     let mut instr_vec: Vec<InstructionValue<'a>> = Vec::new();
//     let mut bb_vec: Vec<BasicBlock<'a>> = Vec::new();
//     let minus = match instr.get_opcode() {
//       InstructionOpcode::Call => {
//         match instr.get_operand(instr.get_num_operands() - 1) {
//           Some(Either::Left(BasicValueEnum::PointerValue(ptr_val))) => {
//             match ptr_val.get_name().to_str().unwrap() {
//               "__assert_fail" => {return (instr_vec, bb_vec, one_global_variable);}
//               _ => {1}
//             }
//           }
//           _ => {1}
//         }
//       }
//       _ => {0}
//     };
//     for i in 0..(instr.get_num_operands() - minus) {
//       let op = instr.get_operand(i);
//       match op {
//         Some(Either::Left(BasicValueEnum::PointerValue(ptr_val))) => {
//           if ptr_val.is_const() {
//             builder.position_before(&instr);
//             match builder.build_direct_call(func, &[BasicMetadataValueEnum::PointerValue(ptr_val)], "globalifyglobal").unwrap().try_as_basic_value() {
//               Either::Left(BasicValueEnum::PointerValue(pv)) => {
//                 instr.set_operand(i, pv);
//                 one_global_variable = true;
//               }
//               _ => {panic!("Unreachable");}
//             };
//           }
//         }
//         Some(Either::Left(BasicValueEnum::IntValue(iv))) => {
//           match iv.as_instruction() {
//             Some(i) => {
//               println!("{:#?}", i);
//               instr_vec.push(i);
//             }
//             _ => {println!("{:#?}", iv);}
//           }
//         }
//         Some(Either::Right(bb)) => {
//           bb_vec.push(bb);
//         }
//         _ => {println!("{:#?}", instr);}
//       }
//     }
//     return (instr_vec, bb_vec, one_global_variable);
//   }

//   fn process_basic_block<'a>(builder: &'a Builder<'a>, func: FunctionValue<'a>, b: BasicBlock<'a>) -> (Vec<BasicBlock<'a>>, bool) {
//     let mut one_global_variable = false;
//     let mut bb_vec: Vec<BasicBlock<'a>> = Vec::new();
//     let mut instr_vec_old: Vec<InstructionValue<'a>> = b.get_instructions().collect();
//     let mut instr_vec_new: Vec<InstructionValue<'a>>;

//     while instr_vec_old.len() != 0 {
//       instr_vec_new = Vec::new();
//       while let Some(instr) = instr_vec_old.pop() {
//         let (mut instr_vec_app, mut bb_vec_app, modified) = GlobalVariablePointerRenamePass::process_instr(builder, func, instr);
//         instr_vec_new.append(&mut instr_vec_app);
//         bb_vec.append(&mut bb_vec_app);
//         one_global_variable |= modified;
//       }
//       std::mem::swap(&mut instr_vec_old, &mut instr_vec_new);
//     }
//     return (bb_vec, one_global_variable);
//   }
// }

// impl LlvmModulePass for GlobalVariablePointerRenamePass {
//   fn run_pass(
//     &self,
//     module: &mut Module,
//     _manager: &ModuleAnalysisManager
//   ) -> PreservedAnalyses {
//     let mut one_global_variable = false;
//     // transform the IR
//     let globalify_func = module.get_function("globalify").unwrap();
//     let cx = module.get_context();
//     let builder = cx.create_builder();
//     let fs = module.get_functions();
//     for f in fs {
//       match f.get_name().to_str().unwrap() {
//         "__pando__replace_load64" => {continue;}
//         "__pando__replace_loadptr" => {continue;}
//         "__pando__replace_store64" => {continue;}
//         "__pando__replace_storeptr" => {continue;}
//         "check_if_global" => {continue;}
//         "deglobalify" => {continue;}
//         "globalify" => {continue;}
//         _ => {}
//       }
//       let mut bb_vec_old: Vec<BasicBlock> = f.get_basic_block_iter().collect();
//       let mut bb_vec_new: Vec<BasicBlock>;
//       while bb_vec_old.len() != 0 {
//         bb_vec_new = Vec::new();
//         while let Some(b) = bb_vec_old.pop() {
//           let (mut bb_vec_app, did_edit) = GlobalVariablePointerRenamePass::process_basic_block(&builder, globalify_func, b);
//           bb_vec_new.append(&mut bb_vec_app);
//           one_global_variable |= did_edit;
//         }
//         std::mem::swap(&mut bb_vec_old, &mut bb_vec_new);
//       }
//     }
//     one_global_variable
//       .then_some(PreservedAnalyses::None)
//       .unwrap_or(PreservedAnalyses::All)
//   }
// }


///////////////////////////////////////////////////////////////////////////////


struct GlobalifyConstsPass;

impl GlobalifyConstsPass {
  fn process_instr(module: &mut Module, builder: &Builder, globalify_func: FunctionValue, instr: InstructionValue) -> bool {
    let mut one_const_globalified = false;
    let num_operands: u32 = instr.get_num_operands();
    eprintln!("\n(globalify-consts pass) -- new instr. num operands is {}. instr is {:?}", num_operands, instr);
    for operand_index in 0 .. num_operands {

      eprintln!("\n  (globalify-consts pass) -- operand #{}, data is {:?}", operand_index, instr.get_operand(operand_index));

      let operand = match instr.get_operand(operand_index) {
        Some(Either::Left(BasicValueEnum::PointerValue(ptr_val))) => ptr_val,
        Some(Either::Left(BasicValueEnum::IntValue(int_val))) => {
          
          eprintln!("  (globalify-consts pass) -- operand is an intvalue, contents are {:?}", int_val);

          one_const_globalified |= match int_val.as_instruction() {
            Some(subinstr) => {
              eprintln!("  (globalify-consts pass) -- after as_instruction() contents are {:?}", subinstr);
              Self::process_instr(module, builder, globalify_func, subinstr)
            }
            None => continue
          };
          continue;
        }
        Some(Either::Right(bb)) => {
          one_const_globalified |= Self::process_basic_block(module, builder, globalify_func, bb);
          continue;
        }
        _ => { continue; }
      };
      
      match module.get_global(operand.get_name().to_str().unwrap()) {
        Some(_) => (),
        None => continue
      }

      eprintln!("  (globalify-consts pass) -- ptr_val is {:?}, ptr_val type is {:?}", operand, operand.get_type());

      eprintln!("  (globalify-consts pass) -- check if it is const...");
      if !operand.is_const() { continue; }

      eprintln!("  (globalify-consts pass) -- check if it has a label name...");
      if operand.get_name().to_str().unwrap() == "" { continue; } 
      
      eprintln!("  (globalify-consts pass) -- ptr + const + labelled. globalify it.");
      one_const_globalified = true;
      builder.position_before(&instr);

      eprintln!("  (globalify-consts pass) -- build globalify_invocation_instr");
      let globalify_invocation_instr = builder
        .build_direct_call(globalify_func, &[BasicMetadataValueEnum::PointerValue(operand)], "globalified_ptr")
        .unwrap()
        .try_as_basic_value()
        .left()
        .unwrap();
    
      eprintln!("  (globalify-consts pass) -- set the operand");
      instr.set_operand(operand_index, globalify_invocation_instr);

      eprintln!("  (globalify-consts pass) -- done!");
    }
    return one_const_globalified;
  } 

  fn process_basic_block(module: &mut Module, builder: &Builder, globalify_func: FunctionValue, b: BasicBlock) -> bool {
    let mut one_const_globalified = false;
    for instr in b.get_instructions() {
      one_const_globalified |= Self::process_instr(module, &builder, globalify_func, instr);
    }
    return one_const_globalified;
  }
}

impl LlvmModulePass for GlobalifyConstsPass {
  fn run_pass(
    &self,
    module: &mut Module,
    _manager: &ModuleAnalysisManager
  ) -> PreservedAnalyses {
    let mut one_const_globalified = false;
    // transform the IR
    let globalify_func = module.get_function("globalify").unwrap();
    let cx = module.get_context();
    let builder = cx.create_builder();
    let fs = module.get_functions();
    for f in fs {
      match f.get_name().to_str().unwrap() {
        "__pando__replace_load64" => continue, 
        "__pando__replace_loadptr" => continue, 
        "__pando__replace_store64" => continue, 
        "__pando__replace_storeptr" => continue, 
        "check_if_global" => continue, 
        "deglobalify" => continue, 
        "globalify" => continue,
        _ => ()
      }
      for b in f.get_basic_block_iter() {
        one_const_globalified |= Self::process_basic_block(module, &builder, globalify_func, b);
      }
    }
    one_const_globalified
    .then_some(PreservedAnalyses::None)
    .unwrap_or(PreservedAnalyses::All)
  }
}
