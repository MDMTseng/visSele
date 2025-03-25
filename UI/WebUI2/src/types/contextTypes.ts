// Types related to context definitions
export type CompParam_GlobalVariable = {
  global_variable: any,
  // global_variable_selector?: () => Promise<string|undefined>,
  set_global_variable: ((path: string[], new_value: any) => void) | undefined,
}; 