#include <string>
#include <jsoncpp/json/json.h>
#include <sstream>
#include <algorithm>
#include <glog/logging.h>
#include <openssl/sha.h>
#include <boost/algorithm/string/replace.hpp>
#include <regex>
#include <curl/curl.h>
#include <util/util.h>

bool match_regex(const std::string value, const std::string re) {
  return regex_match(value, std::regex(re));
}

bool match_like(const std::string value, const std::string re) {
    std::string regex_str(re);
    boost::replace_all(regex_str, "%", ".*");

    return match_regex(value, regex_str);
}

void set_event_value(
    Event & e,
    const std::string & key,
    const std::string & value,
    const bool & replace)
{
  if (key == "host") {
    if (replace || (!e.has_host())) {
      e.set_host(value);
    }
  } else if (key == "service") {
    if (replace || (!e.has_service())) {
      e.set_service(value);
    }
  } else if (key == "description") {
    if (replace || (!e.has_description())) {
      e.set_description(value);
    }
  } else if (key == "state") {
    if (replace || (!e.has_state())) {
      e.set_state(value);
    }
  } else {
    e.set_attr(key, value);
  }
}

void set_event_value(
    Event& e,
    const std::string & key,
    const int & value,
    const bool & replace)
{
  if (key == "metric") {
   if (replace) {
      e.clear_metric();
      e.set_metric_sint64(value);
    } else if (!e.has_metric()) {
      e.set_metric_sint64(value);
    }
  } else if (key == "ttl") {
    if (replace || (!e.has_ttl())) {
      e.set_ttl(value);
    }
  } else if (key == "time") {
    if (replace || (!e.has_time())) {
      e.set_time(value);
    }
  } else {
    LOG(ERROR) << "set_event_value() wrong key: " << key;
  }
}

void set_event_value(
    Event& e,
    const std::string & key,
    const double & value,
    const bool & replace)
{
  if (key == "metric") {
    if (replace) {
      e.clear_metric_sint64();
      e.clear_metric_f();
      e.set_metric_d(value);
    } else if (!e.has_metric()) {
      e.set_metric_d(value);
    }
  } else {
    LOG(ERROR) << "set_event_value() wrong key: " << key;
  }
}

void set_event_value(
    Event & e,
    const std::string & key,
    const boost::variant<std::string, int, double> & val,
    const bool & replace)
{
  switch (val.which()) {
    case 0:
      set_event_value(e, key, boost::get<std::string>(val), replace);
      break;
    case 1:
      set_event_value(e, key, boost::get<int>(val), replace);
      break;
    case 2:
      set_event_value(e, key, boost::get<double>(val), replace);
      break;
  }
}

std::string sha1(const std::string& str) {
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1((const unsigned char*)str.c_str(), (unsigned long)str.size(), hash);
  return std::string((char*)hash, SHA_DIGEST_LENGTH);
}

/* Taken and slightly modified from
   http://en.wikibooks.org/wiki/Algorithm_Implementation/Miscellaneous/Base64 */

const static char encodeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const static char padCharacter = '=';
std::basic_string<char> base64Encode(std::vector<unsigned char> inputBuffer)
{
  std::basic_string<char> encodedString;
  encodedString.reserve(((inputBuffer.size()/3) + (inputBuffer.size() % 3 > 0)) * 4);
  long temp;
  std::vector<unsigned char>::iterator cursor = inputBuffer.begin();
  for(size_t idx = 0; idx < inputBuffer.size()/3; idx++)
  {
    temp  = (*cursor++) << 16; //Convert to big endian
    temp += (*cursor++) << 8;
    temp += (*cursor++);
    encodedString.append(1,encodeLookup[(temp & 0x00FC0000) >> 18]);
    encodedString.append(1,encodeLookup[(temp & 0x0003F000) >> 12]);
    encodedString.append(1,encodeLookup[(temp & 0x00000FC0) >> 6 ]);
    encodedString.append(1,encodeLookup[(temp & 0x0000003F)      ]);
  }
  switch(inputBuffer.size() % 3)
  {
    case 1:
      temp  = (*cursor++) << 16; //Convert to big endian
      encodedString.append(1,encodeLookup[(temp & 0x00FC0000) >> 18]);
      encodedString.append(1,encodeLookup[(temp & 0x0003F000) >> 12]);
      encodedString.append(2,padCharacter);
      break;
    case 2:
      temp  = (*cursor++) << 16; //Convert to big endian
      temp += (*cursor++) << 8;
      encodedString.append(1,encodeLookup[(temp & 0x00FC0000) >> 18]);
      encodedString.append(1,encodeLookup[(temp & 0x0003F000) >> 12]);
      encodedString.append(1,encodeLookup[(temp & 0x00000FC0) >> 6 ]);
      encodedString.append(1,padCharacter);
      break;
  }
  return encodedString;
}

std::string uri_unescape(const std::string& uri) {
  VLOG(3) << "uri_unescape() uri: " << uri;
  CURL *curl = curl_easy_init();

  if (curl == NULL) {
    LOG(ERROR) << "uri_unescape(): curl_easy_init() failed";
    return uri;
  }

  char *ret =  curl_easy_unescape(curl, uri.c_str(), uri.size(), 0);
  VLOG(3) << "*ret " << ret;
  const std::string unescaped(ret);
  curl_free(ret);
  curl_easy_cleanup(curl);
  return unescaped;
}

bool parse_uri(
    const std::string& escaped_uri,
    std::string& index,
    std::map<std::string, std::string>& params )
{
  VLOG(3) << "parse_uri() escaped_uri: " << escaped_uri;
  std::string uri = uri_unescape(escaped_uri);
  VLOG(3) << "parse_uri() uri: " << uri;

  auto it = uri.begin();
  if (*it != '/') {
    LOG(ERROR) << "uri doesn't start with /";
    return false;
  }

  while (it != uri.end() && *it != '?') {
    index += *it;
    it++;
  }

  if (it == uri.end()) {
    LOG(ERROR) << "uri doesn't contain '?'";
    return false;
  }
  it++;

  VLOG(3) << "index name: " << index;

  while(true) {
    std::string key;
    while (it != uri.end() && *it != '=') {
      key += *it;
      it++;
    }

    if (it == uri.end()) {
      LOG(ERROR) << "uri doesn't contain '='";
      return false;
    }
    it++;

    VLOG(3) << "key: " << key;

    std::string value;
    while (it != uri.end() && *it != '&') {
      value += *it;
      it++;
    }

    VLOG(3) << "value: " << value;
    params.insert({key, value});

    if (it == uri.end()) {
      break;
    } else if (*it == '&') {
      it++;
    }
  }

  return true;
}

char**  copy_args(int argc, char **argv) {

  char **copy_argv = (char **)calloc(argc + 1, sizeof(char *));

  for (char **cp = copy_argv; argc > 0; cp++, argv++, argc--) {
    *cp = strdup(*argv);
  }

  return copy_argv;
}

void free_args(int argc, char **argv) {

  char ** aux = argv;

  for (; argc > 0; argv++, argc--) {
    free(*argv);
  }

  free(aux);
}

void ld_environment(char **argv, const std::string dir) {

  if (getenv("LD_LIBRARY_PATH")) {
    return;
  }

  std::string ld_path = "LD_LIBRARY_PATH=" + dir;
  putenv(const_cast<char*>(ld_path.c_str()));

  std::vector<char> binary(1024 * 10, 0);

  if (readlink("/proc/self/exe", &binary[0], binary.size()) < 0) {
    perror("failed to find /proc/self/exe");
    return;
  }

  if (execv(&binary[0], argv) == -1) {
    perror("failed to exec");
    return;
  }
}
