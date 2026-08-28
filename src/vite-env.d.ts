/// <reference types="vite/client" />

// Vite's ambient types, which this project had done without.
//
// Needed for `import.meta.glob`, which `goldenTrace.test.ts` uses to read the
// committed traces as text. The traces stay as `.trace` data files rather than
// becoming a TypeScript module, because the Rust engine port has to read the
// same bytes: a golden that only exists inside a `.ts` file cannot be a
// cross-language contract.
