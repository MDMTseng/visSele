import React, { createContext } from 'react';
import { CompParam_GlobalVariable } from '../types/contextTypes';

// Define the context in its own file for better HMR compatibility
export const ITGlobalVariableContext = createContext<CompParam_GlobalVariable>({
  global_variable: {},
  // Add any other default values needed
  set_global_variable: () => {},
});

// Named export for the provider component for better HMR
export function GlobalVariableProvider({ 
  children, 
  value 
}: { 
  children: React.ReactNode;
  value: CompParam_GlobalVariable;
}) {
  return (
    <ITGlobalVariableContext.Provider value={value}>
      {children}
    </ITGlobalVariableContext.Provider>
  );
} 