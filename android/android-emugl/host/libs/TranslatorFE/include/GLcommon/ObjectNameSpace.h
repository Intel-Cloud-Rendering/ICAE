/*
* Copyright (C) 2016 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "emugl/common/mutex.h"
#include "GLcommon/NamedObject.h"

#include <GLES/gl.h>
#include <unordered_map>
#include <unordered_set>

typedef std::unordered_map<ObjectLocalName, NamedObjectPtr> NamesMap;
typedef std::unordered_map<unsigned int, ObjectLocalName> GlobalToLocalNamesMap;

class GlobalNameSpace;

//
// Class NameSpace - this class manages allocations and deletions of objects
//                   from a single "local" namespace (private to a context share
//                   group). For each allocated object name, a "global" name is
//                   generated as well to be used in the space where all
//                   contexts are shared.
//
//   NOTE: this class does not used by the EGL/GLES layer directly,
//         the EGL/GLES layer creates objects using the ShareGroup class
//         interface (see below).
class NameSpace
{
    friend class ShareGroup;
    friend class GlobalNameSpace;

private:
    NameSpace(NamedObjectType p_type, GlobalNameSpace *globalNameSpace);
    ~NameSpace();

    //
    // genName - creates new object in the namespace and  returns its name.
    //           if genLocal is false then the specified p_localName will be used.
    //           This function also generate a global name for the object,
    //           the value of the global name can be retrieved using the
    //           getGlobalName function.
    //
    ObjectLocalName genName(GenNameInfo genNameInfo, ObjectLocalName p_localName, bool genLocal);

    //
    // getGlobalName - returns the global name of an object or 0 if the object
    //                 does not exist.
    //
    unsigned int getGlobalName(ObjectLocalName p_localName);

    //
    // getLocaalName - returns the local name of an object or 0 if the object
    //                 does not exist.
    //
    ObjectLocalName getLocalName(unsigned int p_globalName);

    //
    // getNamedObject - returns the shared pointer of an object or null if the
    //                  object does not exist.
    NamedObjectPtr getNamedObject(ObjectLocalName p_localName);

    //
    // deleteName - deletes and object from the namespace as well as its
    //              global name from the global name space.
    //
    void deleteName(ObjectLocalName p_localName);

    //
    // isObject - returns true if the named object exist.
    //
    bool isObject(ObjectLocalName p_localName);

    //
    // replaces an object to map to an existing global object
    //
    void replaceGlobalObject(ObjectLocalName p_localName, NamedObjectPtr p_namedObject);

private:
    ObjectLocalName m_nextName = 0;
    NamesMap m_localToGlobalMap;
    GlobalToLocalNamesMap m_globalToLocalMap;
    const NamedObjectType m_type;
    GlobalNameSpace *m_globalNameSpace = nullptr;
};

// Class GlobalNameSpace - this class maintain all global GL object names.
//                         It is contained in the EglDisplay. One emulator has
//                         only one GlobalNameSpace.
class GlobalNameSpace
{
public:
    friend class NamedObject;
private:
    emugl::Mutex m_lock;
};
