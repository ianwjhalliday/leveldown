/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */
#ifndef LD_LEVELDOWN_H
#define LD_LEVELDOWN_H

#include <node.h>
#include <node_jsvmapi.h>
#include <node_buffer.h>
#include <leveldb/slice.h>

static inline size_t StringOrBufferLength(napi_env env, napi_value obj) {
  Napi::HandleScope scope;

  return (obj != nullptr
    && napi_buffer_has_instance(env, obj))
    ? napi_buffer_length(env, obj)
    : napi_get_string_utf8_length(env, obj);
}

static inline void DisposeStringOrBufferFromSlice(
        napi_env env
      , napi_value handle
      , leveldb::Slice slice) {

  if (!slice.empty() && !napi_buffer_has_instance(env, handle))
    delete[] slice.data();
}

// NOTE: must call DisposeStringOrBufferFromSlice() on objects created here
// TODO (ianhall): The use of napi_get_string_utf8 below changes behavior of
// the original leveldown code by adding the v8::String::REPLACE_INVALID_UTF8
// flag to the WriteUtf8 call.  The napi needs to abstract or deal with
// v8::String flags in some way compatible with existing native modules.
#define LD_STRING_OR_BUFFER_TO_SLICE(to, from, name)                           \
  size_t to ## Sz_;                                                            \
  char* to ## Ch_;                                                             \
  {                                                                            \
    napi_valuetype from ## Type_ = napi_get_type_of_value(env, from);          \
    if (from ## Type_ == napi_null || from ## Type_ == napi_undefined) {       \
      to ## Sz_ = 0;                                                           \
      to ## Ch_ = 0;                                                           \
    } else {                                                                   \
      napi_value from ## Object_ = napi_coerce_to_object(env, from);           \
      if (from ## Object_ != nullptr                                           \
          && napi_buffer_has_instance(env, from ## Object_)) {                 \
        to ## Sz_ = napi_buffer_length(env, from ## Object_);                  \
        to ## Ch_ = napi_buffer_data(env, from ## Object_);                    \
      } else {                                                                 \
        napi_value to ## Str_ = napi_coerce_to_string(env, from);              \
        to ## Sz_ = napi_get_string_utf8_length(env, to ## Str_);              \
        to ## Ch_ = new char[to ## Sz_];                                       \
        napi_get_string_utf8(env, to ## Str_, to ## Ch_, -1);                  \
      }                                                                        \
    }                                                                          \
  } \
  leveldb::Slice to(to ## Ch_, to ## Sz_);

#define LD_STRING_OR_BUFFER_TO_COPY(to, from, name)                            \
  size_t to ## Sz_;                                                            \
  char* to ## Ch_;                                                             \
  {                                                                            \
    napi_value from ## Object_ = napi_coerce_to_object(env, from);             \
    if (from ## Object_ != nullptr                                             \
        && napi_buffer_has_instance(env, from ## Object_)) {                   \
      to ## Sz_ = napi_buffer_length(env, from ## Object_);                    \
      to ## Ch_ = new char[to ## Sz_];                                         \
      memcpy(to ## Ch_, napi_buffer_data(env, from ## Object_), to ## Sz_);    \
    } else {                                                                   \
      napi_value to ## Str_ = napi_coerce_to_string(env, from);                \
      to ## Sz_ = napi_get_string_utf8_length(env, to ## Str_);                \
      to ## Ch_ = new char[to ## Sz_];                                         \
      napi_get_string_utf8(env, to ## Str_, to ## Ch_, -1);                    \
    }                                                                          \
  }

// NOTE (ianhall): This macro is never used, but it is converted here for completeness
#define LD_RETURN_CALLBACK_OR_ERROR(callback, msg)                             \
  if (callback != nullptr                                                      \
      && napi_function == napi_get_type_of_value(env, callback)) {             \
    napi_value argv[] = {                                                      \
      napi_create_error(env, napi_create_string(env, msg))                     \
    };                                                                         \
    LD_RUN_CALLBACK(callback, 1, argv)                                         \
    napi_set_return_value(env, info, napi_get_undefined(env));                 \
    return;                                                                    \
  }                                                                            \
  return napi_throw(                                                     \
      env, napi_create_error(env, napi_create_string(env, msg)));

#define LD_RUN_CALLBACK(callback, argc, argv)                                  \
  napi_make_callback(                                                          \
      env, napi_get_global_scope(env), callback, argc, argv);

/* LD_METHOD_SETUP_COMMON setup the following objects:
 *  - Database* database
 *  - napi_value optionsObj (may be empty)
 *  - napi_value callback (won't be empty)
 * Will throw/return if there isn't a callback in arg 0 or 1
 */
// TODO (ianhall): We need a convenience api for throwing an error of any type with a message coming directly from a C string (copy Nan API)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LD_METHOD_SETUP_COMMON(name, optionPos, callbackPos)                   \
  int argsLength = napi_get_cb_args_length(env, info);                         \
  if (argsLength == 0) {                                                       \
    return napi_throw(env,                                               \
      napi_create_error(env,                                                   \
        napi_create_string(env, #name "() requires a callback argument")));    \
  }                                                                            \
  napi_value args[MAX(optionPos+1, callbackPos+1)];                            \
  napi_get_cb_args(env, info, args, MAX(optionPos+1, callbackPos+1));          \
  napi_value _this = napi_get_cb_this(env, info);                              \
  leveldown::Database* database =                                              \
    static_cast<leveldown::Database*>(napi_unwrap(env, _this));                \
  napi_value optionsObj = nullptr;                                             \
  napi_value callback = nullptr;                                               \
  if (optionPos == -1 &&                                                       \
      napi_get_type_of_value(env, args[callbackPos]) == napi_function) {       \
    callback = args[callbackPos];                                              \
  } else if (optionPos != -1 &&                                                \
      napi_get_type_of_value(env, args[callbackPos - 1]) == napi_function) {   \
    callback = args[callbackPos - 1];                                          \
  } else if (optionPos != -1                                                   \
        && napi_get_type_of_value(env, args[optionPos]) == napi_object         \
        && napi_get_type_of_value(env, args[callbackPos]) == napi_function) {  \
    optionsObj = args[optionPos];                                              \
    callback = args[callbackPos];                                              \
  } else {                                                                     \
    return napi_throw(env,                                               \
      napi_create_error(env,                                                   \
        napi_create_string(env, #name "() requires a callback argument")));    \
  }

#define LD_METHOD_SETUP_COMMON_ONEARG(name) LD_METHOD_SETUP_COMMON(name, -1, 0)

#endif
