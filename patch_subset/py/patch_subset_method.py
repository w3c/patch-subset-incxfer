"""Patch and subset PFE method.

Implements the patch and subset PFE method. A font is progressively built up by
asking a server for a set of codepoints. The server responds with a patch that
the client applies to their copy of the font.

This is a python wrapper around a C++ implementation. Uses python ctypes to
interface with the C++ code.
"""
import collections
from ctypes import byref
from ctypes import c_bool
from ctypes import c_char_p
from ctypes import c_int32
from ctypes import c_float
from ctypes import c_uint32
from ctypes import c_ubyte
from ctypes import c_void_p
from ctypes import cdll
from ctypes import POINTER
from ctypes import Structure

from analysis import network_models
from analysis import request_graph

# Define the C interface
patch_subset = cdll.LoadLibrary('./patch_subset/py/patch_subset_session.so')  # pylint: disable=invalid-name


class RECORD(Structure):
  _fields_ = [("request_size", c_uint32), ("response_size", c_uint32)]


new_session = patch_subset.PatchSubsetSession_new  # pylint: disable=invalid-name
new_session.restype = c_void_p
extend = patch_subset.PatchSubsetSession_extend  # pylint: disable=invalid-name
extend.restype = c_bool
get_font_bytes = patch_subset.PatchSubsetSession_get_font  # pylint: disable=invalid-name
get_font_bytes.restype = POINTER(c_ubyte)
get_requests = patch_subset.PatchSubsetSession_get_requests  # pylint: disable=invalid-name
get_requests.restype = POINTER(RECORD)

Record = collections.namedtuple("Record", ["request_size", "response_size"])
Config = collections.namedtuple("Config", [
    "remap_codepoints", "max_predicted_codepoints",
    "prediction_frequency_threshold"
])


def create_with_codepoint_remapping():
  return PatchSubsetMethod(Config(True, 0, 0.0))


def create_with_codepoint_prediction(max_predicted, threshold):
  return PatchSubsetMethod(Config(True, max_predicted, threshold))


def create_without_codepoint_remapping():
  return PatchSubsetMethod(Config(False, 0, 0.0))


class PatchSubsetMethod:
  """Patch and subset PFE method."""

  def __init__(self, config):
    """config is used to configured the underlying client/server."""
    self.config = config

  def name(self):  # pylint: disable=no-self-use
    """Returns the name for this method."""
    modifiers = ""
    if self.config.remap_codepoints:
      modifiers += "_Remapping"
    if self.config.max_predicted_codepoints:
      modifiers += ("_Prediction(%s|%s)" %
                    (self.config.max_predicted_codepoints,
                     self.config.prediction_frequency_threshold))

    return "PatchSubset_PFE%s" % modifiers

  def start_session(self, network_model, font_loader):
    """Starts a new PFE session for this method."""
    del network_model
    return PatchSubsetPfeSession(font_loader, self.config)


class PatchSubsetError(Exception):
  """The patch subset c code failed to execute."""


class FontSession:
  """Tracks the PFE state for a single font.

  Most importantly it keeps a record of what requests where sent
  for each page view for a particular font.
  """

  def __init__(  # pylint: disable=too-many-arguments
      self, font_loader, font_id, page_view_count, config):
    font_dir = font_loader.directory()
    if font_dir and not font_dir.endswith("/"):
      font_dir += "/"
    font_directory_c = c_char_p(font_dir.encode("utf-8"))
    if not font_id:
      font_id = font_loader.default_font() or ""
    font_id_c = c_char_p(font_id.encode("utf-8"))
    self.session = c_void_p(
        new_session(font_directory_c, font_id_c,
                    c_bool(config.remap_codepoints),
                    c_int32(config.max_predicted_codepoints),
                    c_float(config.prediction_frequency_threshold)))
    self.delete_session = patch_subset.PatchSubsetSession_delete
    self.records_by_view = [[]] * page_view_count
    self.last_record_index = None
    self.page_view_count = page_view_count

  def __del__(self):
    if self.session:
      self.delete_session(self.session)
      self.session = None

  def get_records_by_page_view(self, index):
    return self.records_by_view[index]

  def page_viewed(self):
    self.page_view_count += 1
    self.records_by_view.append([])

  def extend(self, codepoints):
    """Extends the tracked font to cover new codepoints.

    codepoints is a list of integer unicode codepoints. Records any
    resulting requests needed to make the extension.
    """
    codepoint_array_c = (c_uint32 * len(codepoints))()
    for idx, codepoint in enumerate(codepoints):
      codepoint_array_c[idx] = codepoint

    if not extend(self.session, codepoint_array_c, c_uint32(len(codepoints))):
      raise PatchSubsetError("Patch subset extend call failed.")

    self.records_by_view[self.page_view_count - 1] = self.get_new_records()
    self.last_record_index = len(self.get_records()) - 1

  def get_font_bytes(self):
    size_c = c_uint32()
    bytes_c = get_font_bytes(self.session, byref(size_c))
    return bytes(bytes_c[i] for i in range(size_c.value))

  def get_new_records(self):
    if self.last_record_index is None:
      return self.get_records()

    return self.get_records()[self.last_record_index + 1:]

  def get_records(self):
    size_c = c_uint32()
    record_array_c = get_requests(self.session, byref(size_c))
    return [
        Record(record_array_c[i].request_size, record_array_c[i].response_size)
        for i in range(size_c.value)
    ]


def to_request_graph(records):
  """Convert a list of records into a request graph.

  In this graph each request depends on the previous request.
  """
  result = set()
  last_request = None
  for record in records:
    request = request_graph.Request(
        network_models.ESTIMATED_HTTP_REQUEST_HEADER_SIZE + record.request_size,
        network_models.ESTIMATED_HTTP_RESPONSE_HEADER_SIZE +
        record.response_size,
        happens_after=last_request)
    last_request = request
    result.add(request)
  return result


class PatchSubsetPfeSession:
  """Fake progressive font enrichment session."""

  def __init__(self, font_loader, config):
    self.sessions_by_font = dict()
    self.page_view_count = 0
    self.font_loader = font_loader
    self.config = config

  def page_view(self, usage_by_font):  # pylint: disable=no-self-use,unused-argument
    """Processes a page view.

    Where one or more fonts are used to render a set of codepoints.
    usage_by_font is a map from font name => a list of codepoints.
    """
    self.page_view_count += 1
    for session in self.sessions_by_font.values():
      session.page_viewed()

    for font_id, usage in usage_by_font.items():
      if font_id not in self.sessions_by_font:
        self.sessions_by_font[font_id] = FontSession(self.font_loader, font_id,
                                                     self.page_view_count,
                                                     self.config)

      self.sessions_by_font[font_id].extend(usage.codepoints)

  def get_request_graphs(self):
    """Returns a graph of requests that would have resulted from the page views.

    Returns a list of request graphs, one per page view.
    """
    result = []
    for i in range(self.page_view_count):
      requests = set()
      for session in self.sessions_by_font.values():
        requests.update(to_request_graph(session.get_records_by_page_view(i)))
      result.append(request_graph.RequestGraph(requests))

    return result

  def get_font_bytes(self, font_id):
    if font_id not in self.sessions_by_font:
      return bytes()

    return self.sessions_by_font[font_id].get_font_bytes()
