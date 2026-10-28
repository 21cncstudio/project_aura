# Aura AQ 1.2.1 beta replacement — 2026-10-28

The owner explicitly requested replacing the old beta on aura-aq.com. Publication completed and verified at approximately 17:33 UTC. Firmware bytes remain those built from 1625a09bc402d34ce6facd87e18993e7b79e4e51.

- Aura 4.3: 1.2.1-beta-1625a09; firmware.bin 3389088 bytes, SHA256 5912583343595e0926da481ae5b1966cf4d7c831e86356f202b52f5294d5e672.
- Aura 7: 1.2.1-beta-1625a09-7-dual-i2c; firmware.bin 3391232 bytes, SHA256 abcf4a7b375c6c2c941d940c8b8676da36b1f8c5be99b4e860bf96b50618d258.

Both existing-key signed ZIPs were imported through Aura Admin after the owner's normal Clerk reverification. Production validated all binaries and Ed25519 signatures. Both releases are published/current, Full + Update. The superseded 2dddfa2 beta releases are blocked; immutable historical packages remain. Stable 1.2.0 for both models and Hub p4.6 are unchanged. No migration, signing-key change, GitHub Release/tag, email, or device write occurred during publication.

Site production: a1dce4ec1c259f9b776720aca015527a92549b6f / dpl_FM4ErcB3iVRpfXMJB574Lb6PjhZy. Backend production: 43341364f314b214ed47709b7ce5b259c7185c4c / dpl_Gj3FgQWBUJLQ7d3cL2yXn9JyD8pf. The backend preserves the latest Hub authorization fix ed283e7f; its only functional change is the exact 7-inch USB beta constant. No device-OTA gate was relaxed.

Release checkouts: D:/21cncstudio/project_aura_aq/release-staging/1.2.1-beta-20261028/site and /link, both on beta-1.2.1-20261028. These are isolated from earlier dirty and archived checkouts.

Validation: 412 site tests, full site ESLint, 155 backend firmware/Hub authorization tests, backend TypeScript/scoped ESLint and both Vercel production builds passed. Staged and public BIN hashes match. Thirteen public checks passed: new downloads/static files, cross-model rejection, old API/static 404s, release-page identities and unchanged Stable 4.3 bytes/7-inch OTA restriction. Authenticated browser checks showed the exact new versions for each model, enabled Update entry, the correct model confirmation, and retained 7-inch wiring warning. No connect/install action was used during publication.

Evidence: PUBLISHED.json, public-verification.json, staged-binary-checks.json and build/test logs in the release-staging directory. Screenshot/admin visible-state evidence is in C:/Users/user/.codex/visualizations/2026/10/28/01a18302-6ccc-79d0-b329-9af938aee81b/.

At publication, exact combined BINs had not been flashed. Existing release notes retain this limitation. The owner then explicitly requested a 4.3-inch Update through the public site; that hardware follow-up is pending and must be recorded separately. 7-inch hardware, cold cycles, long soak and v1.2.0-to-beta transitions remain unqualified.

Rollback references: prior site dpl_2jvbNW2UAnMaWWAcodcxhbAbnPbS, prior backend dpl_FkX3NkNmKcTMoLPNob8zGUa5kKPE. A catalog rollback requires coordinated site/backend identities and release status/current changes; do not blindly restore one component alone.
