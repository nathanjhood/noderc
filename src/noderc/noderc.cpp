/**
 * @file addon.cpp
 * @author Nathan J. Hood (nathanjood@googlemail.com)
 * @brief Exports 'NodeRC' as a binary NodeJS addon.
 * @version 1.0.0
 * @date 2024-01-09
 *
 * @copyright Copyright (c) 2024 Nathan J. Hood (nathanjood@googlemail.com)
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

// Required header and C++ flag
#if __has_include(<napi.h>) && BUILDING_NODE_EXTENSION

// Get standard library dependencies
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

// Get Node Addon API
#include <napi.h>

// Get CMakeRC
#include <cmrc/cmrc.hpp>

// Register our resource library namespace
CMRC_DECLARE(NodeRC::resources);

#ifndef STRINGIFY
# define STRINGIFY_HELPER(n) #n
# define STRINGIFY(n) STRINGIFY_HELPER(n)
#endif

/**
 * @brief The ```NodeRC``` namespace.
 *
 */
namespace NodeRC
{

/** @addtogroup NodeRC
 *  @{
 */


/**
 * @brief The ```NodeRC::binding``` namespace.
 *
 * These functions consume parts of both the ```Napi``` and ```CMakeRC```
 * namespaces, but are not exported as Javascript functions.
 *
 */
namespace binding
{

/** @addtogroup binding
 *  @{
 */

/**
 * @brief Takes the embedded filesystem ```fs``` starting from ```path```, and
 * iterates through it's contents, placing files into the Javascript Object
 * ```obj``` (carrying an ```env```) as a key/value pair.
 *
 * When nested directories are found, the function calls itself recursively to
 * continue drilling downwards through those entries.
 *
 * @param env
 * @param fs
 * @param path
 * @param obj
 * @return true
 * @return false
 */
bool iterate_filesystem(const Napi::Env& env, const cmrc::embedded_filesystem& fs, const std::string& path, const Napi::Object& obj);

bool iterate_filesystem(const Napi::Env& env, const cmrc::embedded_filesystem& fs, const std::string& path, const Napi::Object& obj) {

  using bytes = std::vector<char>;
  bool b = false;

  for (auto&& entry : fs.iterate_directory(path))
  {
    std::string p;
      p += path;
      p += "/";
      p += entry.filename();

    if(entry.is_file()) {

      bytes chunk;

      auto data = fs.open(p);

      chunk.reserve(data.size());

      for (auto i = data.begin(); i != data.end(); i++) {
        chunk.emplace_back(*i);
      }

      obj.Set(
        Napi::String::New(env, entry.filename()),                       // Key
        Napi::String::New(env, std::string(chunk.begin(), chunk.end())) // Val
      );

      p.clear();
      chunk.clear();

    } else if(entry.is_directory())  {

      b = NodeRC::binding::iterate_filesystem(env, fs, p, obj);
      p.clear();

    } else { return false; }
  }

  return true;
}

  /// @} group binding
} // namespace binding

  /// @} group NodeRC
} // namespace NodeRC


namespace Napi {

#ifdef    NAPI_CPP_CUSTOM_NAMESPACE
/**
 * @brief The ```Napi::addon``` namespace.
 *
 * The functions contained in this namespace are exported as Javascript
 * functions, and are generally not intended to be called in other C++ code.
 *
 * @see ```Napi::addon::Init(Napi::Env env, Napi::Object exports)```
 *
 */
namespace NAPI_CPP_CUSTOM_NAMESPACE
{
#endif
/** @addtogroup addon
 *  @{
 */

/**
 * @brief Returns a ```Napi::String```, confirming the module is online.
 *
 * @param info
 * @return Napi::Value
 */
Napi::Value Hello(const Napi::CallbackInfo& info)
{
  return Napi::String::New(info.Env(), STRINGIFY(CMAKEJS_ADDON_NAME)".node is online!");
}

/**
 * @brief Returns a ```Napi::String```, confirming the module version number.
 *
 * @param info
 * @return Napi::Value
 */
Napi::Value Version(const Napi::CallbackInfo& info)
{
  return Napi::Number::New(info.Env(), NAPI_VERSION);
}

/**
 * @brief
 *
 * @param args
 * @return Napi::Value
 */
Napi::Value Open(const Napi::CallbackInfo& args)
{
  Napi::Env env = args.Env();

  // Arguments required: one only
  if (args.Length() != 1)
  {
    Napi::TypeError::New(env, "Wrong number of arguments! Expected exactly one argument.").ThrowAsJavaScriptException();
    return env.Null();
  }

  // Param 1 must be a string
  if (!args[0].IsString())
  {
    Napi::TypeError::New(env, "Wrong argument type! Expected a string.").ThrowAsJavaScriptException();
    return env.Null();
  }

  using bytes = std::vector<int>;
  bytes chunk;

  Napi::String out;

  auto fs = cmrc::NodeRC::resources::get_filesystem();

  try {

    auto data = fs.open(args[0].ToString().Utf8Value());
    chunk.reserve(data.size());

    for (auto i = data.begin(); i != data.end(); i++)
    {
      chunk.emplace_back(*i);
    }

    out = Napi::String::New(env, std::string(chunk.begin(), chunk.end()));

  } catch (const std::system_error& e) {

    // If there was an error...
    if (e.code() == std::errc::no_such_file_or_directory)
    {
      std::string message(e.what());
      message += '\n';
      message += "noderc: No such file or directory: ";
      message += args[0].As<Napi::String>();
      Napi::TypeError::New(env, message).ThrowAsJavaScriptException();
      message.clear();
      return env.Null();
    }

    std::string message(e.what());
    message += '\n';
    message += args[0].As<Napi::String>();
    Napi::TypeError::New(env, message).ThrowAsJavaScriptException();
    message.clear();
    return env.Null();
  }

  return out;
}

/**
 * @brief
 *
 * @param args
 * @return Napi::Value
 */
Napi::Value IsFile(const Napi::CallbackInfo& args)
{
  Napi::Env env = args.Env();

  // Arguments required: one only
  if (args.Length() != 1)
  {
    Napi::TypeError::New(env, "Wrong number of arguments! Expected exactly one argument.").ThrowAsJavaScriptException();
    return env.Null();
  }

  // Param 1 must be a string
  if (!args[0].IsString())
  {
    Napi::TypeError::New(env, "Wrong argument type! Expected a string.").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto fs = cmrc::NodeRC::resources::get_filesystem();

  return Napi::Boolean::New(env, fs.is_file(args[0].ToString().Utf8Value()));
}

/**
 * @brief
 *
 * @param args
 * @return Napi::Value
 */
Napi::Value IsDirectory(const Napi::CallbackInfo& args)
{
  Napi::Env env = args.Env();

  // Arguments required: one only
  if (args.Length() != 1)
  {
    Napi::TypeError::New(env, "Wrong number of arguments! Expected exactly one argument.").ThrowAsJavaScriptException();
    return env.Null();
  }

  // Param 1 must be a string
  if (!args[0].IsString())
  {
    Napi::TypeError::New(env, "Wrong argument type! Expected a string.").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto fs = cmrc::NodeRC::resources::get_filesystem();

  return Napi::Boolean::New(env, fs.is_directory(args[0].ToString().Utf8Value()));
}

/**
 * @brief
 *
 * @param args
 * @return Napi::Value
 */
Napi::Value Exists(const Napi::CallbackInfo& args)
{
  Napi::Env env = args.Env();

  // Arguments required: one only
  if (args.Length() != 1)
  {
    Napi::TypeError::New(env, "Wrong number of arguments! Expected exactly one argument.").ThrowAsJavaScriptException();
    return env.Null();
  }

  // Param 1 must be a string
  if (!args[0].IsString())
  {
    Napi::TypeError::New(env, "Wrong argument type! Expected a string.").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto fs = cmrc::NodeRC::resources::get_filesystem();

  return Napi::Boolean::New(env, fs.exists(args[0].ToString().Utf8Value()));
}

Napi::Value Compare(const Napi::CallbackInfo& args)
{
  Napi::Env env = args.Env();

  // Arguments required: two only
  if (args.Length() != 2)
  {
    Napi::TypeError::New(env, "Wrong number of arguments! Expected exactly two arguments.").ThrowAsJavaScriptException();
    return env.Null();
  }

  // Params must be strings
  if (!args[0].IsString() || !args[1].IsString())
  {
    Napi::TypeError::New(env, "Wrong argument type! Expected a string.").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto arg0 = args[0].ToString().Utf8Value();
  auto arg1 = args[1].ToString().Utf8Value();

  std::ifstream arg0_fs{arg0.data(), std::ios_base::binary};

  if (!arg0_fs)
  {
    Napi::TypeError::New(env, "Invalid filename").ThrowAsJavaScriptException();
    return env.Null();
  }

  using iter         = std::istreambuf_iterator<char>;
  const auto fs_size = std::distance(iter(arg0_fs), iter());
  auto       fs      = cmrc::NodeRC::resources::get_filesystem();

  arg0_fs.seekg(0);

  if (!fs.exists(arg1))
  {
    Napi::TypeError::New(env, "Invalid filename: Does not exist").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (fs.is_directory(arg1))
  {
    Napi::TypeError::New(env, "Invalid filename: Is a directory").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!fs.is_file(arg1))
  {
    Napi::TypeError::New(env, "Invalid filename: Is not a file").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto       arg1_rc   = fs.open(arg1);
  const auto rc_size   = std::distance(arg1_rc.begin(), arg1_rc.end());

  if (rc_size != fs_size)
  {
    // std::cerr << "File sizes do not match: FS == " << fs_size << ", RC == " << rc_size << "\n";
    return Napi::Boolean::New(env, false);
  }

  if (!std::equal(arg1_rc.begin(), arg1_rc.end(), iter(arg0_fs)))
  {
    // std::cerr << "File contents do not match\n";
    return Napi::Boolean::New(env, false);
  }

  // std::cout << "File contents match: FS == " << arg0 << ", RC == " << arg1 << "\n";
  return Napi::Boolean::New(env, true);
}

/**
 * @brief
 *
 * @param args
 * @return Napi::Value
 */
Napi::Value CompareSize(const Napi::CallbackInfo& args)
{
  Napi::Env env = args.Env();

  // Arguments required: two only
  if (args.Length() != 2)
  {
    Napi::TypeError::New(env, "Wrong number of arguments! Expected exactly two arguments.").ThrowAsJavaScriptException();
    return env.Null();
  }

  // Params must be strings
  if (!args[0].IsString() || !args[1].IsString())
  {
    Napi::TypeError::New(env, "Wrong argument type! Expected a string.").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto arg0 = args[0].ToString().Utf8Value();
  auto arg1 = args[1].ToString().Utf8Value();

  std::ifstream arg0_fs{arg0.data(), std::ios_base::binary};

  if (!arg0_fs)
  {
    Napi::TypeError::New(env, "Invalid filename").ThrowAsJavaScriptException();
    return env.Null();
  }

  using iter         = std::istreambuf_iterator<char>;
  const auto fs_size = std::distance(iter(arg0_fs), iter());
  auto       fs      = cmrc::NodeRC::resources::get_filesystem();

  arg0_fs.seekg(0);

  if (!fs.exists(arg1))
  {
    Napi::TypeError::New(env, "Invalid filename: Does not exist").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (fs.is_directory(arg1))
  {
    Napi::TypeError::New(env, "Invalid filename: Is a directory").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!fs.is_file(arg1))
  {
    Napi::TypeError::New(env, "Invalid filename: Is not a file").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto       arg1_rc   = fs.open(arg1);
  const auto rc_size   = std::distance(arg1_rc.begin(), arg1_rc.end());

  if (rc_size != fs_size)
  {
    // std::cerr << "File sizes do not match: FS == " << fs_size << ", RC == " << rc_size << "\n";
    return Napi::Boolean::New(env, false);
  }

  // std::cout << "File sizes match:    FS == " << arg0 << ", RC == " << arg1 << "\n";
  return Napi::Boolean::New(env, true);
}

/**
 * @brief
 *
 * @param args
 * @return Napi::Value
 */
Napi::Value CompareContent(const Napi::CallbackInfo& args)
{
  Napi::Env env = args.Env();

  // Arguments required: two only
  if (args.Length() != 2)
  {
    Napi::TypeError::New(env, "Wrong number of arguments! Expected exactly two arguments.").ThrowAsJavaScriptException();
    return env.Null();
  }

  // Params must be strings
  if (!args[0].IsString() || !args[1].IsString())
  {
    Napi::TypeError::New(env, "Wrong argument type! Expected a string.").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto arg0 = args[0].ToString().Utf8Value();
  auto arg1 = args[1].ToString().Utf8Value();

  std::ifstream arg0_fs{arg0.data(), std::ios_base::binary};

  if (!arg0_fs)
  {
    Napi::TypeError::New(env, "Invalid filename").ThrowAsJavaScriptException();
    return env.Null();
  }

  using iter         = std::istreambuf_iterator<char>;
  auto       fs      = cmrc::NodeRC::resources::get_filesystem();

  arg0_fs.seekg(0);

  if (!fs.exists(arg1))
  {
    Napi::TypeError::New(env, "Invalid filename: Does not exist").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (fs.is_directory(arg1))
  {
    Napi::TypeError::New(env, "Invalid filename: Is a directory").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!fs.is_file(arg1))
  {
    Napi::TypeError::New(env, "Invalid filename: Is not a file").ThrowAsJavaScriptException();
    return env.Null();
  }

  auto       arg1_rc   = fs.open(arg1);

  if (!std::equal(arg1_rc.begin(), arg1_rc.end(), iter(arg0_fs)))
  {
    // std::cerr << "File contents do not match\n";
    return Napi::Boolean::New(env, false);
  }

  // std::cout << "File contents match: FS == " << arg0 << ", RC == " << arg1 << "\n";
  return Napi::Boolean::New(env, true);
}

/**
 * @brief
 *
 * @param args
 * @return Napi::Value
 */
Napi::Value GetFileSystemObject(const Napi::CallbackInfo& args)
{
  Napi::Env env = args.Env();

  // Arguments required: exactly none
  if (args.Length() != 0)
  {
    Napi::TypeError::New(env, "Wrong number of arguments! Expected none.").ThrowAsJavaScriptException();
    return env.Null();
  }

  using bytes = std::vector<char>;

  auto fs = cmrc::NodeRC::resources::get_filesystem();
  auto obj = Napi::Object::New(env);
  const char root[1] = "";

  try {

    auto a = ::NodeRC::binding::iterate_filesystem(env, fs, root, obj);

  } catch (const std::system_error& e) {

    // If there was an error...
    if (e.code() == std::errc::no_such_file_or_directory)
    {
      std::string message(e.what());
      message += '\n';
      message += "noderc: No such file or directory: ";
      message += args[0].As<Napi::String>();
      Napi::TypeError::New(env, message).ThrowAsJavaScriptException();
      message.clear();
      return env.Null();
    }

    std::string message(e.what());
    message += '\n';
    message += args[0].As<Napi::String>();
    Napi::TypeError::New(env, message).ThrowAsJavaScriptException();
    message.clear();
    return env.Null();
  }

  return obj;
}

/**
 * @brief Construct an 'initializer' object that carries our functions.
 *
 * @param env
 * @param exports
 * @return Napi::Object
 */
Napi::Object Init(Napi::Env env, Napi::Object exports)
{
  // Export a chosen C++ function under a given Javascript key
  exports.Set(
    Napi::String::New(env, "hello"), // Name of function on Javascript side...
    Napi::Function::New(env, Hello)  // Name of function on C++ side...
  );

  exports.Set(
    Napi::String::New(env, "version"),
    Napi::Function::New(env, Version)
  );

  exports.Set(
    Napi::String::New(env, "open"),
    Napi::Function::New(env, Open)
  );

  exports.Set(
    Napi::String::New(env, "isFile"),
    Napi::Function::New(env, IsFile)
  );

  exports.Set(
    Napi::String::New(env, "isDirectory"),
    Napi::Function::New(env, IsDirectory)
  );

  exports.Set(
    Napi::String::New(env, "exists"),
    Napi::Function::New(env, Exists)
  );

  exports.Set(
    Napi::String::New(env, "compare"),
    Napi::Function::New(env, Compare)
  );

  exports.Set(
    Napi::String::New(env, "compareSize"),
    Napi::Function::New(env, CompareSize)
  );

  exports.Set(
    Napi::String::New(env, "compareContent"),
    Napi::Function::New(env, CompareContent)
  );

  exports.Set(
    Napi::String::New(env, "getFileSystemObject"),
    Napi::Function::New(env, GetFileSystemObject)
  );

  // The above will expose the C++ function 'Hello' as a javascript function named 'hello', etc...

  return exports;
}

// Register a new addon with the intializer function defined above
NODE_API_MODULE(CMAKEJS_ADDON_NAME, Init) // (name to use, initializer to use)

// The above attaches the functions exported in 'Init()' to the name used in the fist argument.
// The C++ functions are then obtainable on the Javascript side under e.g. 'noderc.hello()'



#ifdef NAPI_CPP_CUSTOM_NAMESPACE
  /// @} group NAPI_CPP_CUSTOM_NAMESPACE
}  // namespace NAPI_CPP_CUSTOM_NAMESPACE
#endif

// Here, we can extend the namepsace with Napi overloads, if we need them.

} // namespace Napi

// Export your custom namespace to outside of the Napi namespace, providing an
// alias to the Napi Addon API; e.g., '<vendor>::<addon>::Object()', along with the
// functions defined above, such as '<vendor>::<addon>::Hello()'.
namespace NAPI_CPP_CUSTOM_NAMESPACE::CMAKEJS_ADDON_NAME {
  using namespace Napi::NAPI_CPP_CUSTOM_NAMESPACE;
}

#else
 #warning "Warning: Cannot find '<napi.h>'"
#endif // __has_include(<napi.h>) && BUILDING_NODE_EXTENSION
