#include "stdafx.h"
#include "FitAbl.h"

// The class's public procedures, its database and output folder (0x11011880-0x11011fb0, 0x11026bd0), and the
// recordset FitAbl's code reads through.

namespace
{
    /// <summary>The caption of the class's error messages.</summary>
    const char* const ERROR_TITLE = "Error";
    /// <summary>The database file's extension, as the class checks and adds it.</summary>
    const char* const DATABASE_EXTENSION = ".mdb";
    const char* const DATABASE_EXTENSION_UPPER = ".MDB";
    /// <summary>The SQLite file the port keeps beside a .mdb.</summary>
    const char* const SQLITE_EXTENSION = ".db";
}

FitAblRecordset::FitAblRecordset(DaoDatabase& database) : DaoRecordset(&database)
{
}

void FitAblRecordset::OpenDynaset(const CString& sql)
{
    // DAO opens a table by its name; the port's recordset needs the SELECT it stands for.
    const bool isQuery = sql.Left(7).CompareNoCase("SELECT ") == 0;
    Open(dbOpenDynaset, isQuery ? sql : "SELECT * FROM [" + sql + "]");
}

void FitAblRecordset::FindFirst(const CString& filter)
{
    noMatch = !DaoRecordset::FindFirst(filter);
}

void FitAblRecordset::FindNext(const CString& filter)
{
    noMatch = !DaoRecordset::FindNext(filter);
}

COleVariant FitAblRecordset::operator[](const char* name)
{
    return GetFieldValue(name);
}

bool FitAbl::WriteMissionFITFile(const CString& outputPath, const CString& databasePath, const CString& databaseName,
                                 short missionId)
{
    try
    {
        return openDatabase(databasePath, databaseName) && setOutputPath(outputPath) && writeFitFile(missionId);
    }
    catch (const VbError& error)
    {
        // An error FitAbl didn't trap ended the call; COM gave its description to the editor.
        Vb::MsgBox(error.description, MB_ICONEXCLAMATION);
    }
    catch (CException* exception)
    {
        char message[512];
        exception->GetErrorMessage(message, sizeof message);
        exception->Delete();
        Vb::MsgBox(message, MB_ICONEXCLAMATION);
    }

    return false;
}

bool FitAbl::openDatabase(const CString& path, const CString& name)
{
    line = path.Right(1) != "\\" ? path + "\\" + name : path + name;

    if (line.Right(4) != DATABASE_EXTENSION && line.Right(4) != DATABASE_EXTENSION_UPPER)
    {
        line += DATABASE_EXTENSION_UPPER;
    }

    // Dir(line) = "": the port also takes the SQLite file it keeps beside the .mdb.
    const CString sqliteFile = line.Left(line.GetLength() - 4) + SQLITE_EXTENSION;

    if (GetFileAttributes(line) == INVALID_FILE_ATTRIBUTES && GetFileAttributes(sqliteFile) == INVALID_FILE_ATTRIBUTES)
    {
        Vb::MsgBox("Could not find file '" + line + "'.", Vb::MSGBOX_CRITICAL, ERROR_TITLE);
        return false;
    }

    databaseFile = line;

    // On Error: DBEngine.OpenDatabase's failure is told and ends the call.
    try
    {
        database.Open(databaseFile);
    }
    catch (CException* exception)
    {
        exception->Delete();
        Vb::MsgBox("Could not open database object '" + line + "'.", Vb::MSGBOX_CRITICAL, ERROR_TITLE);
        return false;
    }

    return true;
}

bool FitAbl::setOutputPath(const CString& path)
{
    line = path.Right(1) != "\\" ? path + "\\" : path;

    // Dir(line, vbDirectory) = "".
    const DWORD attributes = GetFileAttributes(line.Left(line.GetLength() - 1));

    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        Vb::MsgBox("Could not find path '" + line + "'.", Vb::MSGBOX_CRITICAL, ERROR_TITLE);
        return false;
    }

    outputFolder = line;
    return true;
}
