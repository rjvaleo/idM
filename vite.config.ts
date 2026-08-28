import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// The kit gallery entry left with idMLab: the gallery renders idMLab's theme
// kits, so it belongs to the repository that owns them.
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5174,
    open: true,
  },
});
