"""Source-contract checks for the HTTPD-only OTA upload wiring.

Real handlers and error cleanup are exercised by native test_web_ota_handlers.
These checks cover the link into the platform-specific multipart reader; they
do not claim to simulate TCP, HTTPD or physical device behavior.
"""

from pathlib import Path
import re
import unittest


SOURCE = (
    Path(__file__).resolve().parents[2] / "src/web/WebTransportEspHttpServer.cpp"
).read_text(encoding="utf-8")
START = SOURCE.index("size_t uploaded_size = 0;")
END = SOURCE.index("WebUpload end{};", START)
STREAM = SOURCE[START:END]


class OtaTransportContractTests(unittest.TestCase):
    def test_write_callback_stops_reader_when_handler_rejects(self):
        self.assertRegex(
            STREAM,
            r"route\.upload_handler\(\);\s*uploaded_size \+= size;\s*"
            r"return !request_->uploadRejected\(\);",
        )

    def test_rejection_reaches_final_handler_without_becoming_generic_abort(self):
        after_stream = STREAM[STREAM.index("final_boundary);") + len("final_boundary);"):]
        rejected = after_stream.index("if (request_->uploadRejected())")
        aborted = after_stream.index("if (!stream_ok)")
        self.assertLess(rejected, aborted)
        branch = after_stream[rejected:aborted]
        self.assertIn(
            "request_->setPendingBodyBytes(reader.remainingBytesOnSocket());",
            branch,
        )
        self.assertIn("return true;", branch)
        self.assertNotIn("return false;", branch)
        self.assertNotIn("WebUploadStatus::Aborted", branch)
        self.assertNotIn("stopClient", branch)
        self.assertNotIn("resetRequest", branch)

    def test_reader_honors_callback_stop_for_buffer_and_boundary_chunks(self):
        reader_start = SOURCE.index("bool streamUntilBoundary(")
        reader_end = SOURCE.index("bool receiveMore()", reader_start)
        reader = SOURCE[reader_start:reader_end]
        for size_name in ("data_len", "flush_len"):
            pattern = (
                rf"if \({size_name} > 0 && !callback\(buffer_\.data\(\), {size_name}\)\)"
                r"\s*\{\s*return false;\s*\}"
            )
            self.assertRegex(reader, pattern)

    def test_non_rejected_stream_failure_still_calls_aborted_handler(self):
        failure = STREAM[STREAM.index("if (!stream_ok)"):]
        self.assertIn("aborted.status = WebUploadStatus::Aborted;", failure)
        self.assertIn("route.upload_handler();", failure)
        self.assertIn("return true;", failure)
        end = SOURCE[END:SOURCE.index("String value;", END)]
        self.assertIn("end.status = WebUploadStatus::End;", end)
        self.assertIn("route.upload_handler();", end)

    def test_successful_prepare_dispatches_response_before_request_reset(self):
        start = SOURCE.index("if (!route->backend->prepareRequest(*route, req))")
        end = SOURCE.index("static esp_err_t esp_not_found_dispatch", start)
        dispatch = SOURCE[start:end]
        self.assertRegex(
            dispatch,
            r"route->handler\(\);\s*route->backend->finalizeRequest\(\);",
        )


if __name__ == "__main__":
    unittest.main()
