/**
 *  File.cpp
 *
 *  Implementation file for the File class
 *
 *  @author Emiel Bruijntjes <emiel.bruijntjes@copernica.com>
 *  @copyright 2014 Copernica BV
 */

/**
 *  Dependencies
 */
#include "includes.h"

/**
 *  Namespace
 */
namespace Php {

/**
 *  Constructor
 *
 *  The constructor receives a filename as parameter. It uses the normal
 *  PHP include path resolve algorithms to find the location of the file.
 *
 *  @param  name        the filename
 *  @param  size        length of the filename
 */
File::File(const char *name, size_t size) : _original(zend_string_init(name, size, 0))
{
#if PHP_VERSION_ID < 80100
    // resolve the path
    _path = zend_resolve_path(name, size);
#else
    // first convert the path, then read it
    _path = zend_resolve_path(_original);
#endif
}

/**
 *  Destructor
 */
File::~File()
{
    // clean up path name
    if (_path) zend_string_release(_path);

    // clean up original path
    if (_original) zend_string_release(_original);
}

/**
 *  Compile the file
 *  @return bool
 */
bool File::compile()
{
    // never works if the path is invalid
    if (!_path) return false;

    // is the file already compiled?
    if (_opcodes) return _opcodes->valid();

    // we are going to open the file
    zend_file_handle filehandle;

#if PHP_VERSION_ID < 80100

    // open the file
    if (zend_stream_open(ZSTR_VAL(_path), &filehandle) == FAILURE) return false;

#else

    // since php 8 (or 8.1? - this has not been checked), zend_stream_open just takes the file-handle, and we must associate it first with a filename
    zend_stream_init_filename_ex(&filehandle, _path);

    // the stream if supposed to be open by now
    if (zend_stream_open(&filehandle) == FAILURE) return false;

#endif

    // make sure the path name is stored in the handle (@todo: is this necessary? do we need the copy, 
    // this was copied from zend_execute.c, maybe deals with case when opened_path is not set for
    // special types of files that are correctly opened, but that do not expose path-info, while this info
    // is still needed by the subsequent zend_compile_file() call for error messages?)
    if (!filehandle.opened_path) filehandle.opened_path = zend_string_copy(_path);

    // we need temporary compiler options
    CompilerOptions options(ZEND_COMPILE_DEFAULT);

    // create the opcodes
    _opcodes.reset(new Opcodes(zend_compile_file(&filehandle, ZEND_INCLUDE)));

    // close the file handle
    zend_destroy_file_handle(&filehandle);

    // done
    return _opcodes->valid();
}

/**
 *  Does the file exist?
 *  @return boolean
 */
bool File::exists()
{
    // it is of course not valid if the path could not be resolved
    if (!_path) return false;

    // if we have valid opcodes, we're sure that it exists
    if (_opcodes && _opcodes->valid()) return true;

    // retrieve stats
    struct stat buf;
    return stat(ZSTR_VAL(_path), &buf) == 0;
}

/**
 *  Is this a valid file?
 *  @return boolean
 */
bool File::valid()
{
    // check if file is compilable
    return compile();
}

/**
 *  Execute the file
 *  @return Value
 */
Value File::execute()
{
    // do we already have the opcodes?
    if (_opcodes) return _opcodes->execute();

    // try compiling the file
    if (!compile()) return nullptr;

    // execute the opcodes
    return _opcodes->execute();
}

/**
 *  Execute a file only once
 *  @return Value
 */
Value File::once()
{
    // skip if the path is invalid
    if (!_path) return nullptr;

    // check if this file was already included
    if (zend_hash_exists(&EG(included_files), _path)) return nullptr;

    // add the entry to the list of included files
    zend_hash_add_empty_element(&EG(included_files), _path);

    // execute the file
    return execute();
}

/**
 *  End of namespace
 */
}

