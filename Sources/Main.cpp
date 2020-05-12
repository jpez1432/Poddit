
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "Resources.hpp"

std::string Root;

std::string FileSave(HWND hWnd, char *Filter, char *InitPath, char *DefaultExt)
{
    OPENFILENAME ofn;
    char szFile[260];

    ZeroMemory( &ofn, sizeof( ofn ) );
    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof( szFile );
    ofn.lpstrFilter = Filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrDefExt = DefaultExt;
    ofn.lpstrInitialDir = InitPath;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if(GetSaveFileName( &ofn ) == TRUE) {
        return std::string(ofn.lpstrFile);
    } else {
        return "";
    }
}

void ListFiles(std::string Folder, std::string Extensions, std::vector<std::string> &Filenames)
{
    std::string Path = (Folder + "/" + Extensions);

    WIN32_FIND_DATA fd;
    HANDLE hFind = ::FindFirstFile(Path.c_str(), &fd);

    if(hFind != INVALID_HANDLE_VALUE) {
        do {
            if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (Folder == Root) {
                    Filenames.push_back(fd.cFileName);
                } else {
                    Filenames.push_back(Folder + "/" + fd.cFileName);
                }
            } else {
                if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0 ) {
                    ListFiles(Folder + "/" + fd.cFileName, "*.*", Filenames);
                }

            }
        } while(::FindNextFile(hFind, &fd));
        ::FindClose(hFind);
    }

}

bool UpdateReg(HKEY RootKey, LPCTSTR KeyPath, int KeyType, LPCTSTR SubKey, LPCTSTR Value)
{
    HKEY hKey;

//    LONG OpenRes = RegOpenKeyEx(RootKey, KeyPath, 0, KEY_ALL_ACCESS, &hKey);
//    if (OpenRes == ERROR_SUCCESS) {
//        std::cout << "Opened Key" << std::endl;
//    } else {
//        std::cout << "Error Opening Key, Creating Key" << std::endl;
        LONG CreateRes = RegCreateKeyEx(RootKey, KeyPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
        if (CreateRes == ERROR_SUCCESS) {
            std::cout << "Created Key" << std::endl;
        } else {
            std::cout << "Error Creating Key" << std::endl;
            MessageBox(NULL, "Error Adding Poddit To Explorer Right Click Context Menu, Try Running As Administrator!", "Error", MB_ICONEXCLAMATION);
            return false;
        }
//    }

    LONG SetRes = RegSetValueEx(hKey, SubKey, 0, REG_SZ, (LPBYTE)Value, strlen(Value) + 1);
    if (SetRes == ERROR_SUCCESS) {
        std::cout << "Wrote Key" << std::endl;
    } else {
        std::cout << "Error Writing Key" << std::endl;
        return false;
    }

    LONG CloseRes = RegCloseKey(hKey);
    if (CloseRes == ERROR_SUCCESS) {
        std::cout << "Closed Key" << std::endl;
    } else {
        std::cout << "Error Closing Key" << std::endl;
        return false;
    }

    return true;
}

std::ifstream::pos_type GetFileSize(std::string Filename)
{
    std::ifstream In(Filename.c_str(), std::ifstream::ate | std::ifstream::binary);
    std::ifstream::pos_type Size = In.tellg();
    In.close();

    return Size;
}

bool CreatePod(std::string PodFile, std::string PodComment, std::vector<std::string> &Filenames)
{
    int Offset;
    int NumFiles;
    int Filesize;
    char Comment[80];
    char Filename[32];
    std::string Path;

    NumFiles = Filenames.size();
    sprintf(Comment, "%s\n", PodComment.c_str());

    Offset = (84 + (NumFiles) * 40);

    std::ofstream StreamOut(PodFile.c_str(), std::ios::binary | std::ios::out);

    if (StreamOut.fail()) {
        return false;
    }

    StreamOut.write((char*)&NumFiles, sizeof(NumFiles));
    StreamOut.write((char*)&Comment, sizeof(Comment));

    for (int i = 0; i < NumFiles; i++) {

        Filesize = GetFileSize(Filenames[i].c_str());

        if (Filesize == -1) {
            return false;
        }

        if (i > 0) Offset += GetFileSize(Filenames[i - 1].c_str());


        Path = Filenames[i].substr(0, Filenames[i].find_last_of("//"));

        if (Path.find_first_of("//") == std::string::npos) {
            strcpy(Filename, Path.c_str());
        } else {
            Path = Path.substr(Path.find_last_of("//") + 1, Path.length()) + "\\";
            Path += Filenames[i].substr(Filenames[i].find_last_of("//") + 1, Filenames[i].length());
            strcpy(Filename, Path.c_str());
        }

        StreamOut.write((char*)&Filename, sizeof(Filename));
        StreamOut.write((char*)&Filesize, sizeof(Filesize));
        StreamOut.write((char*)&Offset, sizeof(Offset));

    }

    for (int i = 0; i < NumFiles; i++) {

        std::ifstream StreamIn(Filenames[i].c_str(), std::ios::binary | std::ios::in);

        if (StreamIn.fail()) {
            return false;
        }

        StreamIn.seekg(0, std::ios::end);
        std::streamsize Size = StreamIn.tellg();
        StreamIn.seekg(0, std::ios::beg);

        std::vector<char> Buffer(Size);
        StreamIn.read(Buffer.data(), Size);

        StreamOut.write((char*)&Buffer[0], Buffer.size());

        Buffer.clear();
        StreamIn.close();
    }

    StreamOut.close();

    return true;
}


int main(int argc, char *argv[])
{
    TCHAR File[MAX_PATH];
    GetModuleFileName(NULL, File, MAX_PATH);

    Root = std::string(File) + " " + '"' + "%1" + '"' + "\0";

    if (!UpdateReg(HKEY_CLASSES_ROOT, "Folder\\shell\\Save Pod File As...\\command\\", 1, "", Root.c_str())) {
        return 1;
    }

    if (argc == 1) {
        MessageBox(NULL, "No Directory Found, Please Drop A Directory On Poddit", "Input", MB_ICONINFORMATION);
        return 1;
    } else {
        Root = argv[1];
    }

    std::vector<std::string> Filenames;
    ListFiles(Root, "*.*", Filenames);

    std::string PodFile = FileSave( NULL, (char*)("Pod Files\0*.pod\0All Files\0*.*\0"), &std::string("")[0], &std::string(".pod")[0] );

    CreatePod(PodFile, "Compiled With Poddit!", Filenames);

    MessageBox(NULL, "YE", "YE", 0);

    return 0;
}
