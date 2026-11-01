"""Optional CPU C-API fixtures; requires the Python onnx package, no trained models.

python tests/data/generate_topdown_fixtures.py <ignored-output-directory>
Set MIKMAP_TEST_ORT_DIR to the directory containing onnxruntime.dll,
MIKMAP_TEST_TOPDOWN_FIXTURES to that output directory, and optionally
MIKMAP_TEST_HAND_MODELS="<output>/detector-none.onnx;<output>/keypoints-21.onnx".
Run ctest --test-dir build -C Release --output-on-failure.
"""
import sys
from pathlib import Path

import onnx
from onnx import TensorProto, helper


def save(directory, name, input_shape, outputs):
    nodes, infos = [], []
    for output_name, dtype, shape, values in outputs:
        tensor = helper.make_tensor(output_name, dtype, shape, values)
        nodes.append(helper.make_node("Constant", [], [output_name], value=tensor))
        infos.append(helper.make_tensor_value_info(output_name, dtype, shape))
    graph = helper.make_graph(nodes, name, [helper.make_tensor_value_info("input", TensorProto.FLOAT, input_shape)], infos)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)], ir_version=10)
    onnx.checker.check_model(model)
    onnx.save(model, directory / name)


def main():
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    dets = [0, 0, 50, 50, .99, 100, 100, 200, 300, .9,
            300, 100, 400, 300, .8, 0, 0, 50, 50, .95]
    for name, labels in [("detector.onnx", [1099511627793, 0, 0, -1]),
                         ("detector-none.onnx", [1099511627793, 1, 1, -1])]:
        save(directory, name, [1, 3, 640, 640], [
            ("dets", TensorProto.FLOAT, [1, 4, 5], dets),
            ("labels", TensorProto.INT64, [1, 4], labels)])
    for count in (21, 106, 133):
        x, y = [0.] * (count * 384), [0.] * (count * 512)
        for k in range(count):
            x[k * 384 + 192], y[k * 512 + 256] = .8, .6
        save(directory, f"keypoints-{count}.onnx", [1, 3, 256, 192], [
            ("simcc_x", TensorProto.FLOAT, [1, count, 384], x),
            ("simcc_y", TensorProto.FLOAT, [1, count, 512], y)])
    print(f"Checked and wrote five constant ONNX fixtures to {directory}")


if __name__ == "__main__":
    main()
