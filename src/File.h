/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2012  <copyright holder> <email>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef FILE_H
#define FILE_H
#include <fstream>

namespace Steel {

	class FileProvider;
	
	class IFile{
	public:
		virtual bool eof()const=0;
		virtual int  read(char *buffer, int bytes)=0;
		virtual void close()=0;
		virtual bool open(const char* path)=0;
	};
		
	class File : public IFile
	{
	public:
		File();
		virtual ~File();
		virtual bool eof()const;
		virtual int  read(char *buffer, int bytes);
		virtual void close();
		virtual bool open(const char* path);
	private:
		std::ifstream m_in;
	};

	class IFileProvider {
	public:
		IFileProvider(){}
		virtual ~IFileProvider(){}
		virtual IFile * create()=0;
	private:
	};
	
	class FileProvider: public IFileProvider {
	public:
		FileProvider(){}
		virtual ~FileProvider(){}
		virtual IFile * create();		
	private:
	};

}

#endif // FILE_H
