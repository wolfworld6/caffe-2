// This program converts a set of images and annotations to a lmdb/leveldb by
// storing them as AnnotatedDatum proto buffers.
// Usage:
//   convert_annoset [FLAGS] ROOTFOLDER/ LISTFILE DB_NAME
//
// where ROOTFOLDER is the root folder that holds all the images and
// annotations, and LISTFILE should be a list of files as well as their labels
// or label files.
// For classification task, the file should be in the format as
//   imgfolder1/img1.JPEG 7
//   ....
// For detection task, the file should be in the format as
//   imgfolder1/img1.JPEG annofolder1/anno1.xml
//   ....

#include "boost/scoped_ptr.hpp"
#include "boost/variant.hpp"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include <algorithm>
#include <fstream> // NOLINT(readability/streams)
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "caffe/proto/caffe.pb.h"
#include "caffe/util/db.hpp"
#include "caffe/util/format.hpp"
#include "caffe/util/io.hpp"
#include "caffe/util/rng.hpp"

using namespace caffe; // NOLINT(build/namespaces)
using boost::scoped_ptr;
using std::pair;

DEFINE_bool(gray, false,
            "When this option is on, treat images as grayscale ones");
DEFINE_bool(shuffle, false,
            "Randomly shuffle the order of images and their labels");
DEFINE_string(backend, "lmdb",
              "The backend {lmdb, leveldb} for storing the result");
DEFINE_string(anno_type, "classification",
              "The type of annotation {classification, detection}.");
DEFINE_string(label_type, "xml", "The type of annotation file format.");
DEFINE_string(label_map_file, "", "A file with LabelMap protobuf message.");
DEFINE_bool(
    check_label, false,
    "When this option is on, check that there is no duplicated name/label.");
DEFINE_int32(
    min_dim, 0,
    "Minimum dimension images are resized to (keep same aspect ratio)");
DEFINE_int32(
    max_dim, 0,
    "Maximum dimension images are resized to (keep same aspect ratio)");
DEFINE_int32(resize_width, 0, "Width images are resized to");
DEFINE_int32(resize_height, 0, "Height images are resized to");
DEFINE_bool(
    check_size, false,
    "When this option is on, check that all the datum have the same size");
DEFINE_bool(encoded, false,
            "When this option is on, the encoded image will be save in datum");
DEFINE_string(
    encode_type, "",
    "Optional: What type should we encode the image as ('png','jpg',...).");

int main(int argc, char **argv) {
#ifdef USE_OPENCV
  ::google::InitGoogleLogging(argv[0]);
  // Print output to stderr (while still logging)
  FLAGS_alsologtostderr = true;

#ifndef GFLAGS_GFLAGS_H_
  namespace gflags = google;
#endif
  gflags::SetUsageMessage(
      "Convert a set of images and annotations to the "
      "leveldb/lmdb format used as input for Caffe.\n"
      "Usage:\n"
      "    convert_annoset [FLAGS] ROOTFOLDER/ LISTFILE DB_NAME\n");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (argc < 4) {
    gflags::ShowUsageWithFlagsRestrict(argv[0], "tools/convert_annoset");
    return 1;
  }

  const bool is_color = !FLAGS_gray;
  const bool check_size = FLAGS_check_size;
  const bool encoded = FLAGS_encoded;
  const string encode_type = FLAGS_encode_type;
  const string anno_type = FLAGS_anno_type;
  AnnotatedDatum_AnnotationType type;
  const string label_type = FLAGS_label_type;
  const string label_map_file = FLAGS_label_map_file;
  const bool check_label = FLAGS_check_label;
  std::map<std::string, int> name_to_label;

  std::ifstream infile(argv[2]);
  std::vector<std::pair<std::string, boost::variant<int, std::string>>> lines;
  std::vector<std::pair<std::string, std::pair<std::string, std::string>>>
      lines_seg;
  std::string filename;
  int label;
  std::string labelname;
  std::string seglabelname;
  LOG(INFO) << anno_type;

  if (anno_type == "classification") {
    while (infile >> filename >> label) {
      lines.emplace_back(filename, label);
    }
  } else if (anno_type == "detection") {
    type = AnnotatedDatum_AnnotationType_BBOX;

    LabelMap label_map;
    CHECK(ReadProtoFromTextFile(label_map_file, &label_map))
        << "Failed to read label map file.";
    CHECK(MapNameToLabel(label_map, check_label, &name_to_label))
        << "Failed to convert name to label.";
    while (infile >> filename >> labelname) {
      lines.emplace_back(filename, labelname);
    }
  } else if (anno_type == "lane_detection") {

    type = AnnotatedDatum_AnnotationType_LANE;

    while (infile >> filename >> labelname) {
      lines.emplace_back(filename, labelname);
    }
  } else if (anno_type == "detection_with_segmentation") {
    type = AnnotatedDatum_AnnotationType_BBOXandSeg;

    LabelMap label_map;
    CHECK(ReadProtoFromTextFile(label_map_file, &label_map))
        << "Failed to read label map file.";
    CHECK(MapNameToLabel(label_map, check_label, &name_to_label))
        << "Failed to convert name to label.";

    while (infile >> filename >> labelname >> seglabelname) {
      // LOG(INFO)<<filename<<labelname<<seglabelname;
      lines_seg.emplace_back(filename, std::make_pair(labelname, seglabelname));
    }
  }
  if (FLAGS_shuffle) {
    // randomly shuffle data
    LOG(INFO) << "Shuffling data";
    if (anno_type == "detection_with_segmentation") {
      shuffle(lines_seg.begin(), lines_seg.end());
    } else {
      shuffle(lines.begin(), lines.end());
    }
  }
  int lines_size;
  if (anno_type == "detection_with_segmentation") {
    LOG(INFO) << "A total of " << lines_seg.size() << " images.";
    lines_size = lines_seg.size();
  } else {
    LOG(INFO) << "A total of " << lines.size() << " images.";
    lines_size = lines.size();
  }

  if (!encode_type.empty() && !encoded)
    LOG(INFO) << "encode_type specified, assuming encoded=true.";

  int min_dim = std::max<int>(0, FLAGS_min_dim);
  int max_dim = std::max<int>(0, FLAGS_max_dim);
  int resize_height = std::max<int>(0, FLAGS_resize_height);
  int resize_width = std::max<int>(0, FLAGS_resize_width);

  // Create new DB
  scoped_ptr<db::DB> db(db::GetDB(FLAGS_backend));
  db->Open(argv[3], db::NEW);
  scoped_ptr<db::Transaction> txn(db->NewTransaction());

  // Storing to db
  std::string root_folder(argv[1]);
  AnnotatedDatum anno_datum;
  Datum *datum = anno_datum.mutable_datum();
  int count = 0;
  int data_size = 0;
  bool data_size_initialized = false;

  for (int line_id = 0; line_id < lines_size; ++line_id) {
    bool status = true;
    std::string enc = encode_type;
    if (encoded && enc.empty()) {
      // Guess the encoding type from the file name
      string fn;
      if (anno_type == "detection_with_segmentation") {
        fn = lines_seg[line_id].first;
      } else {
        fn = lines[line_id].first;
      }
      size_t p = fn.rfind('.');
      if (p == std::string::npos)
        LOG(WARNING) << "Failed to guess the encoding of '" << fn << "'";
      enc = fn.substr(p);
      std::transform(enc.begin(), enc.end(), enc.begin(), ::tolower);
    }
    if (anno_type == "detection_with_segmentation") {
      filename = root_folder + lines_seg[line_id].first;
    } else {
      filename = root_folder + lines[line_id].first;
    }

    if (anno_type == "classification") {
      label = boost::get<int>(lines[line_id].second);
      status = ReadImageToDatum(filename, label, resize_height, resize_width,
                                min_dim, max_dim, is_color, enc, datum);
    } else if (anno_type == "detection") {
      labelname = root_folder + boost::get<std::string>(lines[line_id].second);
      status = ReadRichImageToAnnotatedDatum(
          filename, labelname, resize_height, resize_width, min_dim, max_dim,
          is_color, enc, type, label_type, name_to_label, &anno_datum);
      anno_datum.set_type(AnnotatedDatum_AnnotationType_BBOX);
    } else if (anno_type == "detection_with_segmentation") {
      // LOG(INFO)<<filename;
      std::pair<std::string, std::string> pair_name = lines_seg[line_id].second;
      // boost::get<std::pair<std::string,std::string>>(lines_seg[line_id].second);
      labelname = root_folder + pair_name.first;
      seglabelname = root_folder + pair_name.second;
      // LOG(INFO)<<labelname;
      status = ReadRichImageToAnnotatedDatumWithSeg(
          filename, labelname, seglabelname, resize_height, resize_width,
          min_dim, max_dim, is_color, enc, type, label_type, name_to_label,
          &anno_datum);
      anno_datum.set_type(AnnotatedDatum_AnnotationType_BBOXandSeg);
    } else if (anno_type == "lane_detection") {

      labelname = root_folder + boost::get<std::string>(lines[line_id].second);
      status = ReadRichImageToAnnotatedDatum(
          filename, labelname, resize_height, resize_width, min_dim, max_dim,
          is_color, enc, type, label_type, name_to_label, &anno_datum);
      anno_datum.set_type(AnnotatedDatum_AnnotationType_LANE);
    }

    if (!status) {

      if (anno_type == "detection_with_segmentation") {
        LOG(WARNING) << "Failed to read " << lines_seg[line_id].first;
      } else {
        LOG(WARNING) << "Failed to read " << lines[line_id].first;
      }
      continue;
    }
    if (check_size) {
      if (!data_size_initialized) {
        data_size = datum->channels() * datum->height() * datum->width();
        data_size_initialized = true;
      } else {
        const std::string &data = datum->data();
        CHECK_EQ(data.size(), data_size)
            << "Incorrect data field size " << data.size();
      }
    }
    // sequential
    string key_str;
    if (anno_type == "detection_with_segmentation") {
      key_str = caffe::format_int(line_id, 8) + "_" + lines_seg[line_id].first;
    } else {
      key_str = caffe::format_int(line_id, 8) + "_" + lines[line_id].first;
    }

    // Put in db
    string out;
    CHECK(anno_datum.SerializeToString(&out));
    txn->Put(key_str, out);

    if (++count % 1000 == 0) {
      // Commit db
      txn->Commit();
      txn.reset(db->NewTransaction());
      LOG(INFO) << "Processed " << count << " files.";
    }
  }
  // write the last batch
  if (count % 1000 != 0) {
    txn->Commit();
    LOG(INFO) << "Processed " << count << " files.";
  }
#else
  LOG(FATAL) << "This tool requires OpenCV; compile with USE_OPENCV.";
#endif // USE_OPENCV
  return 0;
}
