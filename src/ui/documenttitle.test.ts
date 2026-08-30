import { describe, expect, it } from "vitest";
import { transportDocumentTitle } from "./documenttitle";

describe("transport document title", () => {
  it("uses the filename stem and falls back only for an unsaved document", () => {
    expect(transportDocumentTitle(null)).toBe("Untitled");
    expect(transportDocumentTitle("My Piece.idm")).toBe("My Piece");
    expect(transportDocumentTitle("Legacy Piece.idm.json")).toBe("Legacy Piece");
  });
});
