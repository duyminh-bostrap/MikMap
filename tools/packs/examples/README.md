# Camera Body pack templates

These `pack.json` files are incomplete **PackInfo entry templates**, not installable
packs or a catalog. Deliberately invalid size/hash placeholders prevent accidental
publication. No binaries, verified binary hashes or release signing keys are included.
Model version `0.0.0` is a placeholder; choose a release version after validating artifacts.

## Obtain and validate artifacts

- Runtime: [official Windows x64 ONNX Runtime GPU 1.20.1 ZIP](https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-win-x64-gpu-1.20.1.zip).
  Use same-release provider DLLs. Place `onnxruntime.dll`, shared/CUDA providers and
  required dependency DLLs at the **pack root**, because the app uses InstalledPack.dir.
  The [official compatibility table](https://onnxruntime.ai/docs/execution-providers/CUDA-ExecutionProvider.html#requirements)
  pairs ORT 1.20.x with CUDA 12.x / cuDNN 9.x for this build family; CUDA 11.8 /
  cuDNN 8.x is a separate build. Inspect the downloaded DLL imports to select exact
  dependencies. Do not bundle an NVIDIA driver. Test MSVC runtime, driver, CUDA EP
  and session creation on a clean target; loading the core DLL alone is insufficient.
- RTMO: [s export ZIP](https://download.openmmlab.com/mmpose/v1/projects/rtmo/onnx_sdk/rtmo-s_8xb32-600e_body7-640x640-dac2bf74_20231211.zip),
  [m export ZIP](https://download.openmmlab.com/mmpose/v1/projects/rtmo/onnx_sdk/rtmo-m_16xb16-600e_body7-640x640-39e78cc4_20231211.zip),
  mapped by [rtmlib Body](https://github.com/Tau-J/rtmlib/blob/main/rtmlib/tools/solution/body.py).
  Extract the ONNX files; basenames must start `rtmo-s` / `rtmo-m` and end `.onnx`
  (`models/rtmo-s.onnx`, `models/rtmo-m.onnx` work). Verify actual graph outputs
  `dets [1,N,5]`, `keypoints [1,N,17,3]`, types and inference before signing.
  Record original ZIP URL, extracted name, exporter provenance and exact hashes.
  [Official RTMO deployment instructions](https://github.com/open-mmlab/mmpose/blob/main/projects/rtmo/README.md)
  describe re-export through MMDeploy; unrelated RTMPose/raw training outputs do not fit.

Runtime is [MIT](https://github.com/microsoft/onnxruntime/blob/v1.20.1/LICENSE):
include its notice and archive third-party notices. CUDA/cuDNN have separate
[NVIDIA terms](https://docs.nvidia.com/cuda/eula/index.html) and
[cuDNN license requirements](https://docs.nvidia.com/deeplearning/cudnn/backend/latest/reference/support-matrix.html);
retain notices for the exact versions redistributed. RTMO upstream
[MMPose](https://github.com/open-mmlab/mmpose/blob/main/LICENSE) and
[rtmlib](https://github.com/Tau-J/rtmlib/blob/main/LICENSE) code is Apache-2.0.
This does not establish an artifact-specific redistribution grant for all weights
or training datasets. Verify exact weight/dataset terms and retain applicable
LICENSE/NOTICE before redistribution; no commercial-use clearance is claimed here.
License paths in the templates are examples: replace/add the actual notice files.

## Fill, package, sign

Copy a template to `entry.json` and stage the actual files. List **every** runtime
DLL, model and license/notice file with relative `/` paths. For each, fill exact
byte `size` and 64 lowercase hexadecimal `sha256`; set numeric `sizeBytes` to their
sum. Add HTTPS `url` to each catalog file entry for online installation. A catalog
is `{ "packs": [ ...completed entries... ] }`, whereas `pack` takes one entry.
`mikmap_packtool` only offers `keygen`, `sign`, `pack`: it checks supplied hashes
and sizes, but **does not generate them**.

```powershell
# Once for a new identity; output files are overwritten. Keep secret outside repo/staging.
.\mikmap_packtool.exe keygen release-secret.key release-public.pub
(Get-FileHash -LiteralPath .\staging\onnxruntime.dll -Algorithm SHA256).Hash.ToLowerInvariant()
(Get-Item -LiteralPath .\staging\onnxruntime.dll).Length
# Repeat the two measurements for every listed file, then edit entry.json.
.\mikmap_packtool.exe pack release-secret.key entry.json staging runtime-ort-1.20.1.mikpack
.\mikmap_packtool.exe sign release-secret.key catalog.json catalog.json.sig
```

Set the CMake configure/cache settings `MIKMAP_PACK_PUBKEY_HEX` (release public
key) and `MIKMAP_PACK_CATALOG_URL` (signed catalog location), then rebuild the app.
These are compiled into the app, not runtime settings. Publish the signature
at that URL plus `.sig` (for example `catalog.json.sig`). New keys are not
automatically trusted. `sign` signs exact file bytes and writes a newline-terminated
hex Ed25519 signature; do not reformat the catalog afterward. Offline `.mikpack`
also requires the trusted key. Do not copy a C-header hash as a ZIP/DLL hash.

These examples have not downloaded/inspected binaries, run real ONNX inference,
validated dependency imports or cleared redistribution rights. Complete those
release checks before publishing.
