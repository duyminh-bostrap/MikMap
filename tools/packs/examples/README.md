# Camera Body / Hand / Face / Full pack templates

These `pack.json` files are incomplete **PackInfo entry templates**, not installable
packs or a catalog. Deliberately invalid size/hash placeholders prevent accidental
publication. No binaries, verified binary hashes or release signing keys are included.
Model version `0.0.0` is a placeholder; choose a release version after validating artifacts.
Body keeps `pose-rtmo`; new templates `hand-rtm`, `face-rtm`, `wholebody-rtmw`
each require `onnx-runtime-cuda` ≥1.20.1 and list `detector.onnx` + `keypoints.onnx`.
They do not contain models or grant redistribution rights.

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

### New top-down modes: candidates and export contract

These are upstream references and **uninspected candidates**, not compatible
artifacts certified by this project. Preserve Body's verified s/m lookup and
preprocessing. For Hand/Face/Full, inspect/export both graphs before renaming
them to the exact basenames `detector.onnx` and `keypoints.onnx` in the manifest.

| Pack | Keypoint candidate | Detector requirement |
|---|---|---|
| `hand-rtm` | [RTMPose Hand21 export ZIP](https://download.openmmlab.com/mmpose/v1/projects/rtmposev1/onnx_sdk/rtmpose-m_simcc-hand5_pt-aic-coco_210e-256x256-74fb594_20230320.zip) | [RTMDet hand candidate ZIP](https://download.openmmlab.com/mmpose/v1/projects/rtmposev1/onnx_sdk/rtmdet_nano_8xb32-300e_hand-267f9c8f.zip), verify target-specific hand label0 |
| `face-rtm` | [RTMPose Face106 export ZIP](https://download.openmmlab.com/mmpose/v1/projects/rtmposev1/onnx_sdk/rtmpose-m_simcc-face6_pt-in1k_120e-256x256-72a37400_20230529.zip) | **Incomplete:** no compatible RTMDet face detector established; independently source/export a target-specific face label0 detector with provenance |
| `wholebody-rtmw` | [RTMW133 export ZIP](https://download.openmmlab.com/mmpose/v1/projects/rtmw/onnx_sdk/rtmw-dw-l-m_simcc-cocktail14_270e-256x192_20231122.zip) | Independently verified RTMDet person label0; [official person config](https://github.com/open-mmlab/mmpose/blob/main/projects/rtmpose/rtmdet/person/rtmdet_nano_320-8xb32_coco-person.py) and [checkpoint for export](https://download.openmmlab.com/mmpose/v1/projects/rtmpose/rtmdet_nano_8xb32-100e_coco-obj365-person-05d8511e.pth) |

[rtmlib Hand](https://github.com/Tau-J/rtmlib/blob/main/rtmlib/tools/solution/hand.py)
publishes the hand candidates; the [official hand config](https://github.com/open-mmlab/mmpose/blob/main/projects/rtmpose/rtmpose/hand_2d_keypoint/rtmpose-m_8xb32-210e_coco-wholebody-hand-256x256.py)
defines 21 points. [MMPose Face2d](https://github.com/open-mmlab/mmpose/blob/main/projects/rtmpose/README.md#face-2d-106-keypoints)
and its [LaPa config](https://github.com/open-mmlab/mmpose/blob/main/projects/rtmpose/rtmpose/face_2d_keypoint/rtmpose-m_8xb256-120e_lapa-256x256.py)
define **106** landmarks: `face6` denotes six training datasets, not six points.
[rtmlib Wholebody](https://github.com/Tau-J/rtmlib/blob/main/rtmlib/tools/solution/wholebody.py)
uses YOLOX by default; its detector links are **not** substitutes for the RTMDet
normalized preprocessing below. Full's 133 points must use the original layout:
17 body, 6 feet, 68 face, 21 left hand, 21 right hand; do not reorder to OpenPose.

The app's `TopDownPose` contract is deliberately narrower than arbitrary MMPose
exports. Both graphs need one static float32 input `[1,3,H,W]`, batch 1. Only
detector class **0** is used (person for Full, hand/face for target-specific packs).
Required outputs:

| Graph | Output names, types and shapes |
|---|---|
| `detector.onnx` | `dets`: float32 `[1,N,5]` (xyxy pixels in detector input + score), `labels`: **int64** `[1,N]`; end-to-end NMS already applied |
| `keypoints.onnx` | `simcc_x`: float32 `[1,K,2*W]`, `simcc_y`: float32 `[1,K,2*H]`; K=21/106/133 for Hand/Face/Full; split ratio 2, no exported coordinate decoding |

RTMDet input is CHW **BGR**, top-left letterbox with value 114, then
`(value - mean) / std`: mean `(103.53,116.28,123.675)`, std `(57.375,57.12,58.395)`.
The keypoint crop expands the box 1.25× to input aspect ratio, uses affine sampling
with black border, CHW **BGR** normalized by mean `(123.675,116.28,103.53)`,
std `(58.395,57.12,57.375)`. This follows the selected rtmlib convention; a generic
export expecting RGB, raw pixels, embedded normalization or decoded coordinates
does not automatically fit. Compare against the upstream
[RTMDet preprocessing](https://github.com/Tau-J/rtmlib/blob/main/rtmlib/tools/object_detection/rtmdet.py)
and [RTMPose preprocessing](https://github.com/Tau-J/rtmlib/blob/main/rtmlib/tools/pose_estimation/rtmpose.py)
for the exact artifact/revision, and record it. A matching tensor shape alone
does not verify color convention or trained accuracy.

Use [MMPose conversion instructions](https://github.com/open-mmlab/mmpose/blob/main/projects/rtmpose/README.md#%EF%B8%8F-step2-convert-model)
and [MMDeploy export documentation](https://github.com/open-mmlab/mmdeploy/blob/main/docs/en/02-how-to-run/convert_model.md):
select the RTMDet `detection_onnxruntime_static.py` recipe and SimCC
[`pose-detection_simcc_onnxruntime_dynamic.py`](https://github.com/open-mmlab/mmdeploy/blob/main/configs/mmpose/pose-detection_simcc_onnxruntime_dynamic.py)
with `export_postprocess=False`; adapt input dimensions/batch to the chosen model
and the static input contract. Inspect actual names/types/shapes/NMS and compare
known-image outputs before renaming MMDeploy's `end2end.onnx`. Record repository
commit, config, exporter version/options, checkpoint URL/hash, extracted filename,
final ONNX hash and known-image comparison. No detector Face URL is supplied here
because none was verified to satisfy this contract.

Face tracking works without nose metadata; automatic calibration does not.
To enable Face Capture, the publisher must independently verify the anatomical
nose-tip index in that exact 106-point layout and write ONNX custom metadata
`mikmap.face.nose_tip_index` into `keypoints.onnx` as a digits-only decimal string
in 0..105, with provenance explaining the mapping. No such index has been
established here: do not guess 54/80 or copy a synthetic test index. Missing/invalid
metadata disables Capture with a Body-first hint; an existing Body/manual matrix
remains usable on the same camera/image configuration. Verify metadata before
hashing/signing the final file; editing it changes the artifact hash.

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
The new templates list `licenses/Apache-2.0-LICENSE.txt` for upstream code and
`licenses/MODEL-NOTICE.txt` for artifact-specific attribution/terms; include every
applicable LICENSE/NOTICE, not an empty placeholder. In particular,
[LaPa's dataset terms](https://github.com/jd-opensource/lapa-dataset#license) limit
dataset use to non-commercial purposes. This is not a finding about trained-weight
rights either way; obtain/record the actual artifact permissions separately.

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

These templates have not downloaded/inspected their candidate binaries, validated
dependency imports, run trained-model inference or cleared redistribution rights.
Engine tests used ORT 1.26 CPU with synthetic constant-output models; that does not
validate these artifacts or the pinned 1.20.1 CUDA runtime. Complete those release
checks before publishing; see [current test/hardware limits](../../../features.md).
