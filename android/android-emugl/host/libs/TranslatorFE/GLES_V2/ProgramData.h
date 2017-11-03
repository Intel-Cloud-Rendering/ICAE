/*
* Copyright (C) 2011 The Android Open Source Project
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
#ifndef PROGRAM_DATA_H
#define PROGRAM_DATA_H

#include <memory>

class ProgramData:public ObjectData{
public:
    ProgramData();

    GLuint getAttachedVertexShader() const;
    GLuint getAttachedFragmentShader() const;
    GLuint getAttachedComputeShader() const;
    GLuint getAttachedShader(GLenum type) const;

    bool attachShader(GLuint shader,GLenum type);
    bool isAttached(GLuint shader) const;
    bool detachShader(GLuint shader);

    void setLinkStatus(GLint status);
    GLint getLinkStatus() const;

    void setInfoLog(const GLchar *log);
    const GLchar* getInfoLog() const;

    bool isInUse() const { return IsInUse; }
    void setInUse(bool inUse) { IsInUse = inUse; }

    bool getDeleteStatus() const { return DeleteStatus; }
    void setDeleteStatus(bool status) { DeleteStatus = status; }
private:
    GLuint AttachedVertexShader;
    GLuint AttachedFragmentShader;
    GLuint AttachedComputeShader;
    std::unique_ptr<const GLchar[]> infoLog;
    GLint  LinkStatus;
    bool    IsInUse;
    bool    DeleteStatus;
};
#endif
