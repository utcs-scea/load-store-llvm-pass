use llvm_plugin::inkwell::module::Module;
use llvm_plugin::inkwell::types::AnyTypeEnum;
use llvm_plugin::inkwell::values::{BasicMetadataValueEnum, BasicValueEnum, InstructionOpcode, InstructionValue, PointerValue};
use llvm_plugin::{
    LlvmModulePass, ModuleAnalysisManager, PassBuilder, PipelineParsing, PreservedAnalyses
};
use either::Either;

#[llvm_plugin::plugin(name = "scea-load-store-pass", version = "0.1")]
fn plugin_registrar(builder: &mut PassBuilder) {
  builder.add_module_pipeline_parsing_callback(|name, manager| {
    match name {
      "load-store-pass" => {
        manager.add_pass(LoadStorePass);
        PipelineParsing::Parsed
      },
      _ => PipelineParsing::NotParsed,
    }
  });
}

struct LoadStorePass;
impl LlvmModulePass for LoadStorePass {

fn run_pass(&self, module: &mut Module, _manager: &ModuleAnalysisManager) -> PreservedAnalyses {
  let mut one_load_or_store = false;

  let globalify_func = module.get_function("globalify").unwrap();
  let loadsi64_func = module.get_function("__pando__replace_load_int64").unwrap();
  let loadsi32_func = module.get_function("__pando__replace_load_int32").unwrap();
  let loadsi8_func = module.get_function("__pando__replace_load_int8").unwrap();
  let loadsfl32_func = module.get_function("__pando__replace_load_float32").unwrap();
  let loadsptr_func = module.get_function("__pando__replace_load_ptr").unwrap();
  let storei64_func = module.get_function("__pando__replace_store_int64").unwrap();
  let storei32_func = module.get_function("__pando__replace_store_int32").unwrap();
  let storei8_func = module.get_function("__pando__replace_store_int8").unwrap();
  let storefl32_func = module.get_function("__pando__replace_store_float32").unwrap();
  let storeptr_func = module.get_function("__pando__replace_store_ptr").unwrap();

  let cx = module.get_context();
  let builder = cx.create_builder();
  let fs = module.get_functions();

  for f in fs {

    // skip modifying loads/stores inside our wrapper functions
    match f.get_name().to_str().unwrap() {
      "__pando__replace_load_int64" => continue,
      "__pando__replace_load_int32" => continue,
      "__pando__replace_load_int8" => continue,
      "__pando__replace_load_float32" => continue,
      "__pando__replace_load_ptr" => continue,
      "__pando__replace_store_int64" => continue,
      "__pando__replace_store_int32" => continue,
      "__pando__replace_store_int8" => continue,
      "__pando__replace_store_float32" => continue,
      "__pando__replace_store_ptr" => continue,
      "check_if_global" => continue,
      "deglobalify" => continue,
      "globalify" => continue,
      _ => (),
    }

    // iterate over basic blocks in the function
    for b in f.get_basic_block_iter() {

      // iterate over instructions in the basic block
      for instr in b.get_instructions() {

        match instr.get_opcode() {
          InstructionOpcode::Load  => {
            one_load_or_store = true;
            builder.position_at(b, &instr);

            let operand = instr.get_operand(0).unwrap().left().unwrap();

            match operand {
              BasicValueEnum::PointerValue(ptr_val) => {
                // figure out which function we should use to load to this operand
                let func = match instr.get_type() {
                  AnyTypeEnum::PointerType(_) => loadsptr_func,
                  AnyTypeEnum::IntType(int_type) => match int_type.get_bit_width() {
                    64 => loadsi64_func,
                    32 => loadsi32_func,
                    8 => loadsi8_func,
                    _ => {
                      println!(
                        "[LOAD-STORE PASS] we are attempting to instrument a LOAD 
                        with a non-supported bit-width of {}. add this!", 
                        int_type.get_bit_width()
                      );
                      panic!("need to add new supported load behavior")
                    },
                  },
                  AnyTypeEnum::FloatType(_) => loadsfl32_func,
                  AnyTypeEnum::VectorType(_) => loadsptr_func,
                  _ => loadsi64_func,
                };

                // build a call to the chosen loader function to load this operand 
                let replace_instr: InstructionValue = match builder
                  .build_direct_call(func, &[BasicMetadataValueEnum::PointerValue(ptr_val)], "loads_func")
                  .unwrap()
                  .try_as_basic_value() {
                    Either::Left(basic_value) => {
                      if basic_value.is_pointer_value() {
                        basic_value.into_pointer_value().as_instruction().unwrap()
                      } else if basic_value.is_int_value() {
                        basic_value.into_int_value().as_instruction().unwrap()
                      } else if basic_value.is_float_value() {
                        basic_value.into_float_value().as_instruction().unwrap()
                      } else {
                        panic!("This is unreachable for the call type")
                      }
                    }
                    Either::Right(instr_value) => {instr_value}
                };

                // replace the load instruction with our new loader function call.
                instr.replace_all_uses_with(&replace_instr);
                instr.erase_from_basic_block();
              },

              _ => {
                println!(
                  "[LOAD-STORE PASS] we are attempting to load 
                  from something that isn't a pointer! error! {:#?}",
                  operand
                ); 
                panic!("Should not happen");
              },

            }
          }, // end: InstructionOpcode::Load

          InstructionOpcode::Store  => {
            one_load_or_store = true;
            builder.position_at(b, &instr);

            let operand0 = instr.get_operand(0).unwrap().left().unwrap();
            let operand1 = instr.get_operand(1).unwrap().left().unwrap();

            // figure out which function we should use to store this operand
            let func = match operand0 {
              BasicValueEnum::PointerValue(_) => storeptr_func,
              BasicValueEnum::IntValue(int_value) => match int_value.get_type().get_bit_width() {
                64 => storei64_func,
                32 => storei32_func,
                8 => storei8_func,
                _ => {
                  println!(
                    "[LOAD-STORE PASS] we are attempting to instrument a STORE 
                    with a non-supported bit-width of {}. add this!", 
                    int_value.get_type().get_bit_width()
                  );
                  panic!("need to add new supported store behavior")
                },
              },
              BasicValueEnum::FloatValue(_) => storefl32_func,
              _ => {
                panic!("Unreachable {:#?}", operand0)
              },
            };

            // build a call to the chosen storing function to store this operand 
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
                  } else if basic_value.is_float_value() {
                    basic_value.into_float_value().as_instruction().unwrap()
                  } else {
                    panic!("This is unreachable for the call type")
                  }
                },
                Either::Right(instr_value) => instr_value,
            };

            // replace the store instruction with our new storing function call.
            instr.replace_all_uses_with(&replace_instr);
            instr.erase_from_basic_block();
          }, // end: InstructionOpcode::Store

          InstructionOpcode::Alloca => {
            one_load_or_store = true;
            builder.position_at(b, &instr.get_next_instruction().unwrap());

            let ptr_val = PointerValue::try_from(instr).unwrap();

            let next_instr: InstructionValue = match builder
              .build_direct_call(globalify_func, &[BasicMetadataValueEnum::PointerValue(ptr_val)], "globalized")
              .unwrap()
              .try_as_basic_value() {
                Either::Left(basic_value) => basic_value.into_pointer_value().as_instruction().unwrap(),
                Either::Right(instr_value) => instr_value,
            };

            instr.replace_all_uses_with(&next_instr);
            next_instr.set_operand(0, ptr_val);
          }, // end: InstructionOpcode::Alloca

          _ => continue,

        } // end: match instr.get_opcode()

      } // end: iterating over INSTRUCTIONS
    } // end: iterating over BASIC BLOCKS
  } // end: iterating over FUNCTIONS

  one_load_or_store
    .then_some(PreservedAnalyses::None)
    .unwrap_or(PreservedAnalyses::All)
}

}
