/*
 * Win32 ODBC functions
 *
 * Copyright 1999 Xiang Li, Corel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * NOTES:
 *   Proxy ODBC driver manager.  This manager delegates all ODBC
 *   calls to a real ODBC driver manager named by the environment
 *   variable LIB_ODBC_DRIVER_MANAGER, or to libodbc.so if the
 *   variable is not set.
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dlfcn.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/debug.h"
#include "sql.h"
#include "sqltypes.h"
#include "sqlext.h"

#include "unixlib.h"
#include "wine/debug.h"

WINE_DECLARE_DEBUG_CHANNEL(winediag);

static void *libodbc;

static SQLRETURN (*pSQLAllocConnect)(SQLHENV,SQLHDBC*);
static SQLRETURN (*pSQLAllocEnv)(SQLHENV*);
static SQLRETURN (*pSQLAllocHandle)(SQLSMALLINT,SQLHANDLE,SQLHANDLE*);
static SQLRETURN (*pSQLAllocHandleStd)(SQLSMALLINT,SQLHANDLE,SQLHANDLE*);
static SQLRETURN (*pSQLAllocStmt)(SQLHDBC,SQLHSTMT*);
static SQLRETURN (*pSQLBindCol)(SQLHSTMT,SQLUSMALLINT,SQLSMALLINT,SQLPOINTER,SQLLEN,SQLLEN*);
static SQLRETURN (*pSQLBindParam)(SQLHSTMT,SQLUSMALLINT,SQLSMALLINT,SQLSMALLINT,SQLULEN,SQLSMALLINT,SQLPOINTER,SQLLEN*);
static SQLRETURN (*pSQLBindParameter)(SQLHSTMT,SQLUSMALLINT,SQLSMALLINT,SQLSMALLINT,SQLSMALLINT,SQLULEN,SQLSMALLINT,SQLPOINTER,SQLLEN,SQLLEN*);
static SQLRETURN (*pSQLBrowseConnect)(SQLHDBC,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLBrowseConnectW)(SQLHDBC,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLBulkOperations)(SQLHSTMT,SQLSMALLINT);
static SQLRETURN (*pSQLCancel)(SQLHSTMT);
static SQLRETURN (*pSQLCloseCursor)(SQLHSTMT);
static SQLRETURN (*pSQLColAttribute)(SQLHSTMT,SQLUSMALLINT,SQLUSMALLINT,SQLPOINTER,SQLSMALLINT,SQLSMALLINT*,SQLLEN*);
static SQLRETURN (*pSQLColAttributeW)(SQLHSTMT,SQLUSMALLINT,SQLUSMALLINT,SQLPOINTER,SQLSMALLINT,SQLSMALLINT*,SQLLEN*);
static SQLRETURN (*pSQLColAttributes)(SQLHSTMT,SQLUSMALLINT,SQLUSMALLINT,SQLPOINTER,SQLSMALLINT,SQLSMALLINT*,SQLLEN*);
static SQLRETURN (*pSQLColAttributesW)(SQLHSTMT,SQLUSMALLINT,SQLUSMALLINT,SQLPOINTER,SQLSMALLINT,SQLSMALLINT*,SQLLEN*);
static SQLRETURN (*pSQLColumnPrivileges)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLColumnPrivilegesW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLColumns)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLColumnsW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLConnect)(SQLHDBC,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLConnectW)(SQLHDBC,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLCopyDesc)(SQLHDESC,SQLHDESC);
static SQLRETURN (*pSQLDataSources)(SQLHENV,SQLUSMALLINT,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLDataSourcesA)(SQLHENV,SQLUSMALLINT,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLDataSourcesW)(SQLHENV,SQLUSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLDescribeCol)(SQLHSTMT,SQLUSMALLINT,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLSMALLINT*,SQLULEN*,SQLSMALLINT*,SQLSMALLINT*);
static SQLRETURN (*pSQLDescribeColW)(SQLHSTMT,SQLUSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLSMALLINT*,SQLULEN*,SQLSMALLINT*,SQLSMALLINT*);
static SQLRETURN (*pSQLDescribeParam)(SQLHSTMT,SQLUSMALLINT,SQLSMALLINT*,SQLULEN*,SQLSMALLINT*,SQLSMALLINT*);
static SQLRETURN (*pSQLDisconnect)(SQLHDBC);
static SQLRETURN (*pSQLDriverConnect)(SQLHDBC,SQLHWND,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLUSMALLINT);
static SQLRETURN (*pSQLDriverConnectW)(SQLHDBC,SQLHWND,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLUSMALLINT);
static SQLRETURN (*pSQLDrivers)(SQLHENV,SQLUSMALLINT,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLDriversW)(SQLHENV,SQLUSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLEndTran)(SQLSMALLINT,SQLHANDLE,SQLSMALLINT);
static SQLRETURN (*pSQLError)(SQLHENV,SQLHDBC,SQLHSTMT,SQLCHAR*,SQLINTEGER*,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLErrorW)(SQLHENV,SQLHDBC,SQLHSTMT,SQLWCHAR*,SQLINTEGER*,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLExecDirect)(SQLHSTMT,SQLCHAR*,SQLINTEGER);
static SQLRETURN (*pSQLExecDirectW)(SQLHSTMT,SQLWCHAR*,SQLINTEGER);
static SQLRETURN (*pSQLExecute)(SQLHSTMT);
static SQLRETURN (*pSQLExtendedFetch)(SQLHSTMT,SQLUSMALLINT,SQLLEN,SQLULEN*,SQLUSMALLINT*);
static SQLRETURN (*pSQLFetch)(SQLHSTMT);
static SQLRETURN (*pSQLFetchScroll)(SQLHSTMT,SQLSMALLINT,SQLLEN);
static SQLRETURN (*pSQLForeignKeys)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLForeignKeysW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLFreeConnect)(SQLHDBC);
static SQLRETURN (*pSQLFreeEnv)(SQLHENV);
static SQLRETURN (*pSQLFreeHandle)(SQLSMALLINT,SQLHANDLE);
static SQLRETURN (*pSQLFreeStmt)(SQLHSTMT,SQLUSMALLINT);
static SQLRETURN (*pSQLGetConnectAttr)(SQLHDBC,SQLINTEGER,SQLPOINTER,SQLINTEGER,SQLINTEGER*);
static SQLRETURN (*pSQLGetConnectAttrW)(SQLHDBC,SQLINTEGER,SQLPOINTER,SQLINTEGER,SQLINTEGER*);
static SQLRETURN (*pSQLGetConnectOption)(SQLHDBC,SQLUSMALLINT,SQLPOINTER);
static SQLRETURN (*pSQLGetConnectOptionW)(SQLHDBC,SQLUSMALLINT,SQLPOINTER);
static SQLRETURN (*pSQLGetCursorName)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLGetCursorNameW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLGetData)(SQLHSTMT,SQLUSMALLINT,SQLSMALLINT,SQLPOINTER,SQLLEN,SQLLEN*);
static SQLRETURN (*pSQLGetDescField)(SQLHDESC,SQLSMALLINT,SQLSMALLINT,SQLPOINTER,SQLINTEGER,SQLINTEGER*);
static SQLRETURN (*pSQLGetDescFieldW)(SQLHDESC,SQLSMALLINT,SQLSMALLINT,SQLPOINTER,SQLINTEGER,SQLINTEGER*);
static SQLRETURN (*pSQLGetDescRec)(SQLHDESC,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLSMALLINT*,SQLSMALLINT*,SQLLEN*,SQLSMALLINT*,SQLSMALLINT*,SQLSMALLINT*);
static SQLRETURN (*pSQLGetDescRecW)(SQLHDESC,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLSMALLINT*,SQLSMALLINT*,SQLLEN*,SQLSMALLINT*,SQLSMALLINT*,SQLSMALLINT*);
static SQLRETURN (*pSQLGetDiagField)(SQLSMALLINT,SQLHANDLE,SQLSMALLINT,SQLSMALLINT,SQLPOINTER,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLGetDiagFieldW)(SQLSMALLINT,SQLHANDLE,SQLSMALLINT,SQLSMALLINT,SQLPOINTER,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLGetDiagRec)(SQLSMALLINT,SQLHANDLE,SQLSMALLINT,SQLCHAR*,SQLINTEGER*,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLGetDiagRecA)(SQLSMALLINT,SQLHANDLE,SQLSMALLINT,SQLCHAR*,SQLINTEGER*, SQLCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLGetDiagRecW)(SQLSMALLINT,SQLHANDLE,SQLSMALLINT,SQLWCHAR*,SQLINTEGER*,SQLWCHAR*,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLGetEnvAttr)(SQLHENV,SQLINTEGER,SQLPOINTER,SQLINTEGER,SQLINTEGER*);
static SQLRETURN (*pSQLGetFunctions)(SQLHDBC,SQLUSMALLINT,SQLUSMALLINT*);
static SQLRETURN (*pSQLGetInfo)(SQLHDBC,SQLUSMALLINT,SQLPOINTER,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLGetInfoW)(SQLHDBC,SQLUSMALLINT,SQLPOINTER,SQLSMALLINT,SQLSMALLINT*);
static SQLRETURN (*pSQLGetStmtAttr)(SQLHSTMT,SQLINTEGER,SQLPOINTER,SQLINTEGER,SQLINTEGER*);
static SQLRETURN (*pSQLGetStmtAttrW)(SQLHSTMT,SQLINTEGER,SQLPOINTER,SQLINTEGER,SQLINTEGER*);
static SQLRETURN (*pSQLGetStmtOption)(SQLHSTMT,SQLUSMALLINT,SQLPOINTER);
static SQLRETURN (*pSQLGetTypeInfo)(SQLHSTMT,SQLSMALLINT);
static SQLRETURN (*pSQLGetTypeInfoW)(SQLHSTMT,SQLSMALLINT);
static SQLRETURN (*pSQLMoreResults)(SQLHSTMT);
static SQLRETURN (*pSQLNativeSql)(SQLHDBC,SQLCHAR*,SQLINTEGER,SQLCHAR*,SQLINTEGER,SQLINTEGER*);
static SQLRETURN (*pSQLNativeSqlW)(SQLHDBC,SQLWCHAR*,SQLINTEGER,SQLWCHAR*,SQLINTEGER,SQLINTEGER*);
static SQLRETURN (*pSQLNumParams)(SQLHSTMT,SQLSMALLINT*);
static SQLRETURN (*pSQLNumResultCols)(SQLHSTMT,SQLSMALLINT*);
static SQLRETURN (*pSQLParamData)(SQLHSTMT,SQLPOINTER*);
static SQLRETURN (*pSQLParamOptions)(SQLHSTMT,SQLULEN,SQLULEN*);
static SQLRETURN (*pSQLPrepare)(SQLHSTMT,SQLCHAR*,SQLINTEGER);
static SQLRETURN (*pSQLPrepareW)(SQLHSTMT,SQLWCHAR*,SQLINTEGER);
static SQLRETURN (*pSQLPrimaryKeys)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLPrimaryKeysW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLProcedureColumns)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLProcedureColumnsW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLProcedures)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLProceduresW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLPutData)(SQLHSTMT,SQLPOINTER,SQLLEN);
static SQLRETURN (*pSQLRowCount)(SQLHSTMT,SQLLEN*);
static SQLRETURN (*pSQLSetConnectAttr)(SQLHDBC,SQLINTEGER,SQLPOINTER,SQLINTEGER);
static SQLRETURN (*pSQLSetConnectAttrW)(SQLHDBC,SQLINTEGER,SQLPOINTER,SQLINTEGER);
static SQLRETURN (*pSQLSetConnectOption)(SQLHDBC,SQLUSMALLINT,SQLULEN);
static SQLRETURN (*pSQLSetConnectOptionW)(SQLHDBC,SQLUSMALLINT,SQLULEN);
static SQLRETURN (*pSQLSetCursorName)(SQLHSTMT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLSetCursorNameW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLSetDescField)(SQLHDESC,SQLSMALLINT,SQLSMALLINT,SQLPOINTER,SQLINTEGER);
static SQLRETURN (*pSQLSetDescFieldW)(SQLHDESC,SQLSMALLINT,SQLSMALLINT,SQLPOINTER,SQLINTEGER);
static SQLRETURN (*pSQLSetDescRec)(SQLHDESC,SQLSMALLINT,SQLSMALLINT,SQLSMALLINT,SQLLEN,SQLSMALLINT,SQLSMALLINT,SQLPOINTER,SQLLEN*,SQLLEN*);
static SQLRETURN (*pSQLSetEnvAttr)(SQLHENV,SQLINTEGER,SQLPOINTER,SQLINTEGER);
static SQLRETURN (*pSQLSetParam)(SQLHSTMT,SQLUSMALLINT,SQLSMALLINT,SQLSMALLINT,SQLULEN,SQLSMALLINT,SQLPOINTER,SQLLEN*);
static SQLRETURN (*pSQLSetPos)(SQLHSTMT,SQLSETPOSIROW,SQLUSMALLINT,SQLUSMALLINT);
static SQLRETURN (*pSQLSetScrollOptions)(SQLHSTMT,SQLUSMALLINT,SQLLEN,SQLUSMALLINT);
static SQLRETURN (*pSQLSetStmtAttr)(SQLHSTMT,SQLINTEGER,SQLPOINTER,SQLINTEGER);
static SQLRETURN (*pSQLSetStmtAttrW)(SQLHSTMT,SQLINTEGER,SQLPOINTER,SQLINTEGER);
static SQLRETURN (*pSQLSetStmtOption)(SQLHSTMT,SQLUSMALLINT,SQLULEN);
static SQLRETURN (*pSQLSpecialColumns)(SQLHSTMT,SQLUSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLUSMALLINT,SQLUSMALLINT);
static SQLRETURN (*pSQLSpecialColumnsW)(SQLHSTMT,SQLUSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLUSMALLINT,SQLUSMALLINT);
static SQLRETURN (*pSQLStatistics)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLUSMALLINT,SQLUSMALLINT);
static SQLRETURN (*pSQLStatisticsW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLUSMALLINT,SQLUSMALLINT);
static SQLRETURN (*pSQLTablePrivileges)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLTablePrivilegesW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLTables)(SQLHSTMT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLTablesW)(SQLHSTMT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT,SQLWCHAR*,SQLSMALLINT);
static SQLRETURN (*pSQLTransact)(SQLHENV,SQLHDBC,SQLUSMALLINT);


static NTSTATUS load_odbc(void);

static NTSTATUS odbc_process_attach( void *args )
{
    if (!load_odbc()) return STATUS_DLL_NOT_FOUND;
    return STATUS_SUCCESS;
}

static NTSTATUS odbc_process_detach( void *args )
{
    if (libodbc) dlclose( libodbc );
    libodbc = NULL;
    return STATUS_SUCCESS;
}

static NTSTATUS wrap_SQLAllocConnect( void *args )
{
    struct SQLAllocConnect_params *params = args;

    return pSQLAllocConnect(params->EnvironmentHandle, params->ConnectionHandle);
}

static NTSTATUS wrap_SQLAllocEnv( void *args )
{
    struct SQLAllocEnv_params *params = args;

    return pSQLAllocEnv(params->EnvironmentHandle);
}

static NTSTATUS wrap_SQLAllocHandle( void *args )
{
    struct SQLAllocHandle_params *params = args;

    return pSQLAllocHandle(params->HandleType, params->InputHandle, params->OutputHandle);
}

static NTSTATUS wrap_SQLAllocHandleStd( void *args )
{
    struct SQLAllocHandleStd_params *params = args;

    return pSQLAllocHandleStd(params->HandleType, params->InputHandle, params->OutputHandle);
}

static NTSTATUS wrap_SQLAllocStmt( void *args )
{
    struct SQLAllocStmt_params *params = args;

    return pSQLAllocStmt(params->ConnectionHandle, params->StatementHandle);
}

static NTSTATUS wrap_SQLBindCol( void *args )
{
    struct SQLBindCol_params *params = args;

    return pSQLBindCol(params->StatementHandle, params->ColumnNumber, params->TargetType,
                       params->TargetValue, params->BufferLength, params->StrLen_or_Ind);
}

static NTSTATUS wrap_SQLBindParam( void *args )
{
    struct SQLBindParam_params *params = args;

    return pSQLBindParam(params->StatementHandle, params->ParameterNumber, params->ValueType,
                         params->ParameterType, params->LengthPrecision, params->ParameterScale,
                         params->ParameterValue, params->StrLen_or_Ind);
}

static NTSTATUS wrap_SQLBindParameter( void *args )
{
    struct SQLBindParameter_params *params = args;

    return pSQLBindParameter(params->hstmt, params->ipar, params->fParamType, params->fCType,
                             params->fSqlType, params->cbColDef, params->ibScale, params->rgbValue,
                             params->cbValueMax, params->pcbValue);
}

static NTSTATUS wrap_SQLBrowseConnect( void *args )
{
    struct SQLBrowseConnect_params *params = args;

    return pSQLBrowseConnect(params->hdbc, params->szConnStrIn, params->cbConnStrIn, params->szConnStrOut,
                             params->cbConnStrOutMax, params->pcbConnStrOut);
}

static NTSTATUS wrap_SQLBrowseConnectW( void *args )
{
    struct SQLBrowseConnectW_params *params = args;

    return pSQLBrowseConnectW(params->hdbc, params->szConnStrIn, params->cbConnStrIn, params->szConnStrOut,
                              params->cbConnStrOutMax, params->pcbConnStrOut);
}

static NTSTATUS wrap_SQLBulkOperations( void *args )
{
    struct SQLBulkOperations_params *params = args;

    return pSQLBulkOperations(params->StatementHandle, params->Operation);
}

static NTSTATUS wrap_SQLCancel( void *args )
{
    struct SQLCancel_params *params = args;

    return pSQLCancel(params->StatementHandle);
}

static NTSTATUS wrap_SQLCloseCursor( void *args )
{
    struct SQLCloseCursor_params *params = args;

    return pSQLCloseCursor(params->StatementHandle);
}

static NTSTATUS wrap_SQLColAttribute( void *args )
{
    struct SQLColAttribute_params *params = args;

    return pSQLColAttribute(params->StatementHandle, params->ColumnNumber, params->FieldIdentifier,
                            params->CharacterAttribute, params->BufferLength, params->StringLength,
                            params->NumericAttribute);
}

static NTSTATUS wrap_SQLColAttributeW( void *args )
{
    struct SQLColAttributeW_params *params = args;

    return pSQLColAttributeW(params->StatementHandle, params->ColumnNumber, params->FieldIdentifier,
                             params->CharacterAttribute, params->BufferLength, params->StringLength,
                             params->NumericAttribute);
}

static NTSTATUS wrap_SQLColAttributes( void *args )
{
    struct SQLColAttributes_params *params = args;

    return pSQLColAttributes(params->hstmt, params->icol, params->fDescType, params->rgbDesc,
                             params->cbDescMax, params->pcbDesc, params->pfDesc);
}

static NTSTATUS wrap_SQLColAttributesW( void *args )
{
    struct SQLColAttributesW_params *params = args;

    return pSQLColAttributesW(params->hstmt, params->icol, params->fDescType, params->rgbDesc,
                              params->cbDescMax, params->pcbDesc, params->pfDesc);
}

static NTSTATUS wrap_SQLColumnPrivileges( void *args )
{
    struct SQLColumnPrivileges_params *params = args;

    return pSQLColumnPrivileges(params->hstmt, params->szCatalogName, params->cbCatalogName,
                                params->szSchemaName, params->cbSchemaName, params->szTableName,
                                params->cbTableName, params->szColumnName, params->cbColumnName);
}

static NTSTATUS wrap_SQLColumnPrivilegesW( void *args )
{
    struct SQLColumnPrivilegesW_params *params = args;

    return pSQLColumnPrivilegesW(params->hstmt, params->szCatalogName, params->cbCatalogName,
                                 params->szSchemaName, params->cbSchemaName, params->szTableName,
                                 params->cbTableName, params->szColumnName, params->cbColumnName);
}

static NTSTATUS wrap_SQLColumns( void *args )
{
    struct SQLColumns_params *params = args;

    return pSQLColumns(params->StatementHandle, params->CatalogName, params->NameLength1,
                       params->SchemaName, params->NameLength2, params->TableName, params->NameLength3,
                       params->ColumnName, params->NameLength4);
}

static NTSTATUS wrap_SQLColumnsW( void *args )
{
    struct SQLColumnsW_params *params = args;

    return pSQLColumnsW(params->StatementHandle, params->CatalogName, params->NameLength1,
                        params->SchemaName, params->NameLength2, params->TableName, params->NameLength3,
                        params->ColumnName, params->NameLength4);
}

static NTSTATUS wrap_SQLConnect( void *args )
{
    struct SQLConnect_params *params = args;

    return pSQLConnect(params->ConnectionHandle, params->ServerName, params->NameLength1, params->UserName,
                       params->NameLength2, params->Authentication, params->NameLength3);
}

static NTSTATUS wrap_SQLConnectW( void *args )
{
    struct SQLConnectW_params *params = args;

    return pSQLConnectW(params->ConnectionHandle, params->ServerName, params->NameLength1,
                        params->UserName, params->NameLength2, params->Authentication, params->NameLength3);
}

static NTSTATUS wrap_SQLCopyDesc( void *args )
{
    struct SQLCopyDesc_params *params = args;

    return pSQLCopyDesc(params->SourceDescHandle, params->TargetDescHandle);
}

static NTSTATUS wrap_SQLDataSources( void *args )
{
    struct SQLDataSources_params *params = args;

    return pSQLDataSources(params->EnvironmentHandle, params->Direction, params->ServerName,
                           params->BufferLength1, params->NameLength1, params->Description,
                           params->BufferLength2, params->NameLength2);
}

static NTSTATUS wrap_SQLDataSourcesA( void *args )
{
    struct SQLDataSourcesA_params *params = args;

    return pSQLDataSourcesA(params->EnvironmentHandle, params->Direction, params->ServerName,
                            params->BufferLength1, params->NameLength1, params->Description,
                            params->BufferLength2, params->NameLength2);
}

static NTSTATUS wrap_SQLDataSourcesW( void *args )
{
    struct SQLDataSourcesW_params *params = args;

    return pSQLDataSourcesW(params->EnvironmentHandle, params->Direction, params->ServerName,
                            params->BufferLength1, params->NameLength1, params->Description,
                            params->BufferLength2, params->NameLength2);
}

static NTSTATUS wrap_SQLDescribeCol( void *args )
{
    struct SQLDescribeCol_params *params = args;

    return pSQLDescribeCol(params->StatementHandle, params->ColumnNumber, params->ColumnName,
                           params->BufferLength, params->NameLength, params->DataType,
                           params->ColumnSize, params->DecimalDigits, params->Nullable);
}

static NTSTATUS wrap_SQLDescribeColW( void *args )
{
    struct SQLDescribeColW_params *params = args;

    return pSQLDescribeColW(params->StatementHandle, params->ColumnNumber, params->ColumnName,
                            params->BufferLength, params->NameLength, params->DataType,
                            params->ColumnSize, params->DecimalDigits, params->Nullable);
}

static NTSTATUS wrap_SQLDescribeParam( void *args )
{
    struct SQLDescribeParam_params *params = args;

    return pSQLDescribeParam(params->hstmt, params->ipar, params->pfSqlType, params->pcbParamDef,
                             params->pibScale, params->pfNullable);
}

static NTSTATUS wrap_SQLDisconnect( void *args )
{
    struct SQLDisconnect_params *params = args;

    return pSQLDisconnect(params->ConnectionHandle);
}

static NTSTATUS wrap_SQLDriverConnect( void *args )
{
    struct SQLDriverConnect_params *params = args;

    return pSQLDriverConnect(params->hdbc, params->hwnd, params->ConnectionString, params->Length,
                             params->conn_str_out, params->conn_str_out_max,
                             params->ptr_conn_str_out, params->driver_completion);
}

static NTSTATUS wrap_SQLDriverConnectW( void *args )
{
    struct SQLDriverConnectW_params *params = args;

    return pSQLDriverConnectW(params->ConnectionHandle, params->WindowHandle, params->InConnectionString,
                              params->Length, params->OutConnectionString, params->BufferLength,
                              params->Length2, params->DriverCompletion);
}

static NTSTATUS wrap_SQLDrivers( void *args )
{
    struct SQLDrivers_params *params = args;

    return pSQLDrivers(params->EnvironmentHandle, params->fDirection, params->szDriverDesc,
                       params->cbDriverDescMax, params->pcbDriverDesc, params->szDriverAttributes,
                       params->cbDriverAttrMax, params->pcbDriverAttr);
}

static NTSTATUS wrap_SQLDriversW( void *args )
{
    struct SQLDriversW_params *params = args;

    return pSQLDriversW(params->EnvironmentHandle, params->fDirection, params->szDriverDesc,
                        params->cbDriverDescMax, params->pcbDriverDesc, params->szDriverAttributes,
                        params->cbDriverAttrMax, params->pcbDriverAttr);
}

static NTSTATUS wrap_SQLEndTran( void *args )
{
    struct SQLEndTran_params *params = args;

    return pSQLEndTran(params->HandleType, params->Handle, params->CompletionType);
}

static NTSTATUS wrap_SQLError( void *args )
{
    struct SQLError_params *params = args;

    return pSQLError(params->EnvironmentHandle, params->ConnectionHandle, params->StatementHandle,
                     params->Sqlstate, params->NativeError, params->MessageText,
                     params->BufferLength, params->TextLength);
}

static NTSTATUS wrap_SQLErrorW( void *args )
{
    struct SQLErrorW_params *params = args;

    return pSQLErrorW(params->EnvironmentHandle, params->ConnectionHandle, params->StatementHandle,
                      params->Sqlstate, params->NativeError, params->MessageText,
                      params->BufferLength, params->TextLength);
}

static NTSTATUS wrap_SQLExecDirect( void *args )
{
    struct SQLExecDirect_params *params = args;

    return pSQLExecDirect(params->StatementHandle, params->StatementText, params->TextLength);
}

static NTSTATUS wrap_SQLExecDirectW( void *args )
{
    struct SQLExecDirectW_params *params = args;

    return pSQLExecDirectW(params->StatementHandle, params->StatementText, params->TextLength);
}

static NTSTATUS wrap_SQLExecute( void *args )
{
    struct SQLExecute_params *params = args;

    return pSQLExecute(params->StatementHandle);
}

static NTSTATUS wrap_SQLExtendedFetch( void *args )
{
    struct SQLExtendedFetch_params *params = args;

    return pSQLExtendedFetch(params->hstmt, params->fFetchType, params->irow,
                             params->pcrow, params->rgfRowStatus);
}

static NTSTATUS wrap_SQLFetch( void *args )
{
    struct SQLFetch_params *params = args;

    return pSQLFetch(params->StatementHandle);
}

static NTSTATUS wrap_SQLFetchScroll( void *args )
{
    struct SQLFetchScroll_params *params = args;

    return pSQLFetchScroll(params->StatementHandle, params->FetchOrientation, params->FetchOffset);
}

static NTSTATUS wrap_SQLForeignKeys( void *args )
{
    struct SQLForeignKeys_params *params = args;

    return pSQLForeignKeys(params->hstmt, params->szPkCatalogName, params->cbPkCatalogName,
                           params->szPkSchemaName, params->cbPkSchemaName, params->szPkTableName,
                           params->cbPkTableName, params->szFkCatalogName, params->cbFkCatalogName,
                           params->szFkSchemaName, params->cbFkSchemaName, params->szFkTableName,
                           params->cbFkTableName);
}

static NTSTATUS wrap_SQLForeignKeysW( void *args )
{
    struct SQLForeignKeysW_params *params = args;

    return pSQLForeignKeysW(params->hstmt, params->szPkCatalogName, params->cbPkCatalogName,
                            params->szPkSchemaName, params->cbPkSchemaName, params->szPkTableName,
                            params->cbPkTableName, params->szFkCatalogName, params->cbFkCatalogName,
                            params->szFkSchemaName, params->cbFkSchemaName, params->szFkTableName,
                            params->cbFkTableName);
}

static NTSTATUS wrap_SQLFreeConnect( void *args )
{
    struct SQLFreeConnect_params *params = args;

    return pSQLFreeConnect(params->ConnectionHandle);
}

static NTSTATUS wrap_SQLFreeEnv( void *args )
{
    struct SQLFreeEnv_params *params = args;

    return pSQLFreeEnv(params->EnvironmentHandle);
}

static NTSTATUS wrap_SQLFreeHandle( void *args )
{
    struct SQLFreeHandle_params *params = args;

    return pSQLFreeHandle(params->HandleType, params->Handle);
}

static NTSTATUS wrap_SQLFreeStmt( void *args )
{
    struct SQLFreeStmt_params *params = args;

    return pSQLFreeStmt(params->StatementHandle, params->Option);
}

static NTSTATUS wrap_SQLGetConnectAttr( void *args )
{
    struct SQLGetConnectAttr_params *params = args;

    return pSQLGetConnectAttr(params->ConnectionHandle, params->Attribute, params->Value,
                              params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetConnectAttrW( void *args )
{
    struct SQLGetConnectAttrW_params *params = args;

    return pSQLGetConnectAttrW(params->ConnectionHandle, params->Attribute, params->Value,
                               params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetConnectOption( void *args )
{
    struct SQLGetConnectOption_params *params = args;

    return pSQLGetConnectOption(params->ConnectionHandle, params->Option, params->Value);
}

static NTSTATUS wrap_SQLGetConnectOptionW( void *args )
{
    struct SQLGetConnectOptionW_params *params = args;

    return pSQLGetConnectOptionW(params->ConnectionHandle, params->Option, params->Value);
}

static NTSTATUS wrap_SQLGetCursorName( void *args )
{
    struct SQLGetCursorName_params *params = args;

    return pSQLGetCursorName(params->StatementHandle, params->CursorName, params->BufferLength,
                             params->NameLength);
}

static NTSTATUS wrap_SQLGetCursorNameW( void *args )
{
    struct SQLGetCursorNameW_params *params = args;

    return pSQLGetCursorNameW(params->StatementHandle, params->CursorName, params->BufferLength,
                              params->NameLength);
}

static NTSTATUS wrap_SQLGetData( void *args )
{
    struct SQLGetData_params *params = args;

    return pSQLGetData(params->StatementHandle, params->ColumnNumber, params->TargetType,
                       params->TargetValue, params->BufferLength, params->StrLen_or_Ind);
}

static NTSTATUS wrap_SQLGetDescField( void *args )
{
    struct SQLGetDescField_params *params = args;

    return pSQLGetDescField(params->DescriptorHandle, params->RecNumber, params->FieldIdentifier,
                            params->Value, params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetDescFieldW( void *args )
{
    struct SQLGetDescFieldW_params *params = args;

    return pSQLGetDescFieldW(params->DescriptorHandle, params->RecNumber, params->FieldIdentifier,
                             params->Value, params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetDescRec( void *args )
{
    struct SQLGetDescRec_params *params = args;

    return pSQLGetDescRec(params->DescriptorHandle, params->RecNumber, params->Name, params->BufferLength,
                          params->StringLength, params->Type, params->SubType, params->Length,
                          params->Precision, params->Scale, params->Nullable);
}

static NTSTATUS wrap_SQLGetDescRecW( void *args )
{
    struct SQLGetDescRecW_params *params = args;

    return pSQLGetDescRecW(params->DescriptorHandle, params->RecNumber, params->Name, params->BufferLength,
                           params->StringLength, params->Type, params->SubType, params->Length,
                           params->Precision, params->Scale, params->Nullable);
}

static NTSTATUS wrap_SQLGetDiagField( void *args )
{
    struct SQLGetDiagField_params *params = args;

    return pSQLGetDiagField(params->HandleType, params->Handle, params->RecNumber, params->DiagIdentifier,
                            params->DiagInfo, params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetDiagFieldW( void *args )
{
    struct SQLGetDiagFieldW_params *params = args;

    return pSQLGetDiagFieldW(params->HandleType, params->Handle, params->RecNumber, params->DiagIdentifier,
                             params->DiagInfo, params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetDiagRec( void *args )
{
    struct SQLGetDiagRec_params *params = args;

    return pSQLGetDiagRec(params->HandleType, params->Handle, params->RecNumber, params->Sqlstate,
                          params->NativeError, params->MessageText, params->BufferLength,
                          params->TextLength);
}

static NTSTATUS wrap_SQLGetDiagRecA( void *args )
{
    struct SQLGetDiagRecA_params *params = args;

    return pSQLGetDiagRecA(params->HandleType, params->Handle, params->RecNumber, params->Sqlstate,
                           params->NativeError, params->MessageText, params->BufferLength,
                           params->TextLength);
}

static NTSTATUS wrap_SQLGetDiagRecW( void *args )
{
    struct SQLGetDiagRecW_params *params = args;

    return pSQLGetDiagRecW(params->HandleType, params->Handle, params->RecNumber, params->Sqlstate,
                           params->NativeError, params->MessageText, params->BufferLength,
                           params->TextLength);
}

static NTSTATUS wrap_SQLGetEnvAttr( void *args )
{
    struct SQLGetEnvAttr_params *params = args;

    return pSQLGetEnvAttr(params->EnvironmentHandle, params->Attribute, params->Value,
                          params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetFunctions( void *args )
{
    struct SQLGetFunctions_params *params = args;

    return pSQLGetFunctions(params->ConnectionHandle, params->FunctionId, params->Supported);
}

static NTSTATUS wrap_SQLGetInfo( void *args )
{
    struct SQLGetInfo_params *params = args;

    return pSQLGetInfo(params->ConnectionHandle, params->InfoType, params->InfoValue,
                       params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetInfoW( void *args )
{
    struct SQLGetInfoW_params *params = args;

    return pSQLGetInfoW(params->ConnectionHandle, params->InfoType, params->InfoValue,
                        params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetStmtAttr( void *args )
{
    struct SQLGetStmtAttr_params *params = args;

    return pSQLGetStmtAttr(params->StatementHandle, params->Attribute, params->Value,
                           params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetStmtAttrW( void *args )
{
    struct SQLGetStmtAttrW_params *params = args;

    return pSQLGetStmtAttrW(params->StatementHandle, params->Attribute, params->Value,
                            params->BufferLength, params->StringLength);
}

static NTSTATUS wrap_SQLGetStmtOption( void *args )
{
    struct SQLGetStmtOption_params *params = args;

    return pSQLGetStmtOption(params->StatementHandle, params->Option, params->Value);
}

static NTSTATUS wrap_SQLGetTypeInfo( void *args )
{
    struct SQLGetTypeInfo_params *params = args;

    return pSQLGetTypeInfo(params->StatementHandle, params->DataType);
}

static NTSTATUS wrap_SQLGetTypeInfoW( void *args )
{
    struct SQLGetTypeInfoW_params *params = args;

    return pSQLGetTypeInfoW(params->StatementHandle, params->DataType);
}

static NTSTATUS wrap_SQLMoreResults( void *args )
{
    struct SQLMoreResults_params *params = args;

    return pSQLMoreResults(params->StatementHandle);
}

static NTSTATUS wrap_SQLNativeSql( void *args )
{
    struct SQLNativeSql_params *params = args;

    return pSQLNativeSql(params->hdbc, params->szSqlStrIn, params->cbSqlStrIn, params->szSqlStr,
                         params->cbSqlStrMax, params->pcbSqlStr);
}

static NTSTATUS wrap_SQLNativeSqlW( void *args )
{
    struct SQLNativeSqlW_params *params = args;

    return pSQLNativeSqlW(params->hdbc, params->szSqlStrIn, params->cbSqlStrIn, params->szSqlStr,
                          params->cbSqlStrMax, params->pcbSqlStr);
}

static NTSTATUS wrap_SQLNumParams( void *args )
{
    struct SQLNumParams_params *params = args;

    return pSQLNumParams(params->hstmt, params->pcpar);
}

static NTSTATUS wrap_SQLNumResultCols( void *args )
{
    struct SQLNumResultCols_params *params = args;

    return pSQLNumResultCols(params->StatementHandle, params->ColumnCount);
}

static NTSTATUS wrap_SQLParamData( void *args )
{
    struct SQLParamData_params *params = args;

    return pSQLParamData(params->StatementHandle, params->Value);
}

static NTSTATUS wrap_SQLParamOptions( void *args )
{
    struct SQLParamOptions_params *params = args;

    return pSQLParamOptions(params->hstmt, params->crow, params->pirow);
}

static NTSTATUS wrap_SQLPrepare( void *args )
{
    struct SQLPrepare_params *params = args;

    return pSQLPrepare(params->StatementHandle, params->StatementText, params->TextLength);
}

static NTSTATUS wrap_SQLPrepareW( void *args )
{
    struct SQLPrepareW_params *params = args;

    return pSQLPrepareW(params->StatementHandle, params->StatementText, params->TextLength);
}

static NTSTATUS wrap_SQLPrimaryKeys( void *args )
{
    struct SQLPrimaryKeys_params *params = args;

    return pSQLPrimaryKeys(params->hstmt, params->szCatalogName, params->cbCatalogName,
                           params->szSchemaName, params->cbSchemaName,
                           params->szTableName, params->cbTableName);
}

static NTSTATUS wrap_SQLPrimaryKeysW( void *args )
{
    struct SQLPrimaryKeysW_params *params = args;

    return pSQLPrimaryKeysW(params->hstmt, params->szCatalogName, params->cbCatalogName,
                            params->szSchemaName, params->cbSchemaName,
                            params->szTableName, params->cbTableName);
}

static NTSTATUS wrap_SQLProcedureColumns( void *args )
{
    struct SQLProcedureColumns_params *params = args;

    return pSQLProcedureColumns(params->hstmt, params->szCatalogName, params->cbCatalogName,
                                params->szSchemaName, params->cbSchemaName, params->szProcName,
                                params->cbProcName, params->szColumnName, params->cbColumnName);
}

static NTSTATUS wrap_SQLProcedureColumnsW( void *args )
{
    struct SQLProcedureColumnsW_params *params = args;

    return pSQLProcedureColumnsW(params->hstmt, params->szCatalogName, params->cbCatalogName,
                                 params->szSchemaName, params->cbSchemaName, params->szProcName,
                                 params->cbProcName, params->szColumnName, params->cbColumnName);
}

static NTSTATUS wrap_SQLProcedures( void *args )
{
    struct SQLProcedures_params *params = args;

    return pSQLProcedures(params->hstmt, params->szCatalogName, params->cbCatalogName,
                          params->szSchemaName, params->cbSchemaName, params->szProcName,
                          params->cbProcName);
}

static NTSTATUS wrap_SQLProceduresW( void *args )
{
    struct SQLProceduresW_params *params = args;

    return pSQLProceduresW(params->hstmt, params->szCatalogName, params->cbCatalogName,
                           params->szSchemaName, params->cbSchemaName, params->szProcName,
                           params->cbProcName);
}

static NTSTATUS wrap_SQLPutData( void *args )
{
    struct SQLPutData_params *params = args;

    return pSQLPutData(params->StatementHandle, params->Data, params->StrLen_or_Ind);
}

static NTSTATUS wrap_SQLRowCount( void *args )
{
    struct SQLRowCount_params *params = args;

    return pSQLRowCount(params->StatementHandle, params->RowCount);
}

static NTSTATUS wrap_SQLSetConnectAttr( void *args )
{
    struct SQLSetConnectAttr_params *params = args;

    return pSQLSetConnectAttr(params->ConnectionHandle, params->Attribute, params->Value,
                              params->StringLength);
}

static NTSTATUS wrap_SQLSetConnectAttrW( void *args )
{
    struct SQLSetConnectAttrW_params *params = args;

    return pSQLSetConnectAttrW(params->ConnectionHandle, params->Attribute, params->Value,
                               params->StringLength);
}

static NTSTATUS wrap_SQLSetConnectOption( void *args )
{
    struct SQLSetConnectOption_params *params = args;

    return pSQLSetConnectOption(params->ConnectionHandle, params->Option, params->Value);
}

static NTSTATUS wrap_SQLSetConnectOptionW( void *args )
{
    struct SQLSetConnectOptionW_params *params = args;

    return pSQLSetConnectOptionW(params->ConnectionHandle, params->Option, params->Value);
}

static NTSTATUS wrap_SQLSetCursorName( void *args )
{
    struct SQLSetCursorName_params *params = args;

    return pSQLSetCursorName(params->StatementHandle, params->CursorName, params->NameLength);
}

static NTSTATUS wrap_SQLSetCursorNameW( void *args )
{
    struct SQLSetCursorNameW_params *params = args;

    return pSQLSetCursorNameW(params->StatementHandle, params->CursorName, params->NameLength);
}

static NTSTATUS wrap_SQLSetDescField( void *args )
{
    struct SQLSetDescField_params *params = args;

    return pSQLSetDescField(params->DescriptorHandle, params->RecNumber, params->FieldIdentifier,
                            params->Value, params->BufferLength);
}

static NTSTATUS wrap_SQLSetDescFieldW( void *args )
{
    struct SQLSetDescFieldW_params *params = args;

    return pSQLSetDescFieldW(params->DescriptorHandle, params->RecNumber, params->FieldIdentifier,
                             params->Value, params->BufferLength);
}

static NTSTATUS wrap_SQLSetDescRec( void *args )
{
    struct SQLSetDescRec_params *params = args;

    return pSQLSetDescRec(params->DescriptorHandle, params->RecNumber, params->Type, params->SubType,
                          params->Length, params->Precision, params->Scale, params->Data,
                          params->StringLength, params->Indicator);
}

static NTSTATUS wrap_SQLSetEnvAttr( void *args )
{
    struct SQLSetEnvAttr_params *params = args;

    return pSQLSetEnvAttr(params->EnvironmentHandle, params->Attribute, params->Value, params->StringLength);
}

static NTSTATUS wrap_SQLSetParam( void *args )
{
    struct SQLSetParam_params *params = args;

    return pSQLSetParam(params->StatementHandle, params->ParameterNumber, params->ValueType,
                        params->ParameterType, params->LengthPrecision, params->ParameterScale,
                        params->ParameterValue, params->StrLen_or_Ind);
}

static NTSTATUS wrap_SQLSetPos( void *args )
{
    struct SQLSetPos_params *params = args;

    return pSQLSetPos(params->hstmt, params->irow, params->fOption, params->fLock);
}

static NTSTATUS wrap_SQLSetScrollOptions( void *args )
{
    struct SQLSetScrollOptions_params *params = args;

    return pSQLSetScrollOptions(params->statement_handle, params->f_concurrency,
                                params->crow_keyset, params->crow_rowset);
}

static NTSTATUS wrap_SQLSetStmtAttr( void *args )
{
    struct SQLSetStmtAttr_params *params = args;

    return pSQLSetStmtAttr(params->StatementHandle, params->Attribute, params->Value, params->StringLength);
}

static NTSTATUS wrap_SQLSetStmtAttrW( void *args )
{
    struct SQLSetStmtAttrW_params *params = args;

    return pSQLSetStmtAttrW(params->StatementHandle, params->Attribute, params->Value, params->StringLength);
}

static NTSTATUS wrap_SQLSetStmtOption( void *args )
{
    struct SQLSetStmtOption_params *params = args;

    return pSQLSetStmtOption(params->StatementHandle, params->Option, params->Value);
}

static NTSTATUS wrap_SQLSpecialColumns( void *args )
{
    struct SQLSpecialColumns_params *params = args;

    return pSQLSpecialColumns(params->StatementHandle, params->IdentifierType, params->CatalogName,
                              params->NameLength1, params->SchemaName, params->NameLength2,
                              params->TableName, params->NameLength3, params->Scope, params->Nullable);
}

static NTSTATUS wrap_SQLSpecialColumnsW( void *args )
{
    struct SQLSpecialColumnsW_params *params = args;

    return pSQLSpecialColumnsW(params->StatementHandle, params->IdentifierType, params->CatalogName,
                               params->NameLength1, params->SchemaName, params->NameLength2,
                               params->TableName, params->NameLength3, params->Scope, params->Nullable);
}

static NTSTATUS wrap_SQLStatistics( void *args )
{
    struct SQLStatistics_params *params = args;

    return pSQLStatistics(params->StatementHandle, params->CatalogName, params->NameLength1,
                          params->SchemaName, params->NameLength2, params->TableName,
                          params->NameLength3, params->Unique, params->Reserved);
}

static NTSTATUS wrap_SQLStatisticsW( void *args )
{
    struct SQLStatisticsW_params *params = args;

    return pSQLStatisticsW(params->StatementHandle, params->CatalogName, params->NameLength1,
                           params->SchemaName, params->NameLength2, params->TableName,
                           params->NameLength3, params->Unique, params->Reserved);
}

static NTSTATUS wrap_SQLTablePrivileges( void *args )
{
    struct SQLTablePrivileges_params *params = args;

    return pSQLTablePrivileges(params->hstmt, params->szCatalogName, params->cbCatalogName,
                               params->szSchemaName, params->cbSchemaName, params->szTableName,
                               params->cbTableName);
}

static NTSTATUS wrap_SQLTablePrivilegesW( void *args )
{
    struct SQLTablePrivilegesW_params *params = args;

    return pSQLTablePrivilegesW(params->hstmt, params->szCatalogName, params->cbCatalogName,
                                params->szSchemaName, params->cbSchemaName, params->szTableName,
                                params->cbTableName);
}

static NTSTATUS wrap_SQLTables( void *args )
{
    struct SQLTables_params *params = args;

    return pSQLTables(params->StatementHandle, params->CatalogName, params->NameLength1,
                      params->SchemaName, params->NameLength2, params->TableName,
                      params->NameLength3, params->TableType, params->NameLength4);
}

static NTSTATUS wrap_SQLTablesW( void *args )
{
    struct SQLTablesW_params *params = args;

    return pSQLTablesW(params->StatementHandle, params->CatalogName, params->NameLength1,
                       params->SchemaName, params->NameLength2, params->TableName,
                       params->NameLength3, params->TableType, params->NameLength4);
}

static NTSTATUS wrap_SQLTransact( void *args )
{
    struct SQLTransact_params *params = args;

    return pSQLTransact(params->EnvironmentHandle, params->ConnectionHandle, params->CompletionType);
}

unixlib_entry_t __wine_unix_call_funcs[NB_ODBC_FUNCS] =
{
    odbc_process_attach,
    odbc_process_detach,
};

static NTSTATUS error_func( void *args )
{
    return SQL_ERROR;
}

static NTSTATUS load_odbc(void)
{
   const char *s = getenv("LIB_ODBC_DRIVER_MANAGER");

#ifdef SONAME_LIBODBC
   if (!s || !s[0]) s = SONAME_LIBODBC;
#endif
   if (!s || !s[0] || !(libodbc = dlopen( s, RTLD_LAZY | RTLD_GLOBAL )))
   {
       ERR_(winediag)("failed to open library %s: %s\n", debugstr_a(s), dlerror());
       return STATUS_DLL_NOT_FOUND;
   }

#define LOAD_FUNC(name) \
   __wine_unix_call_funcs[unix_##name] = (p##name = dlsym( libodbc, #name )) ? wrap_##name : error_func

    LOAD_FUNC(SQLAllocConnect);
    LOAD_FUNC(SQLAllocEnv);
    LOAD_FUNC(SQLAllocHandle);
    LOAD_FUNC(SQLAllocHandleStd);
    LOAD_FUNC(SQLAllocStmt);
    LOAD_FUNC(SQLBindCol);
    LOAD_FUNC(SQLBindParam);
    LOAD_FUNC(SQLBindParameter);
    LOAD_FUNC(SQLBrowseConnect);
    LOAD_FUNC(SQLBrowseConnectW);
    LOAD_FUNC(SQLBulkOperations);
    LOAD_FUNC(SQLCancel);
    LOAD_FUNC(SQLCloseCursor);
    LOAD_FUNC(SQLColAttribute);
    LOAD_FUNC(SQLColAttributeW);
    LOAD_FUNC(SQLColAttributes);
    LOAD_FUNC(SQLColAttributesW);
    LOAD_FUNC(SQLColumnPrivileges);
    LOAD_FUNC(SQLColumnPrivilegesW);
    LOAD_FUNC(SQLColumns);
    LOAD_FUNC(SQLColumnsW);
    LOAD_FUNC(SQLConnect);
    LOAD_FUNC(SQLConnectW);
    LOAD_FUNC(SQLCopyDesc);
    LOAD_FUNC(SQLDataSources);
    LOAD_FUNC(SQLDataSourcesA);
    LOAD_FUNC(SQLDataSourcesW);
    LOAD_FUNC(SQLDescribeCol);
    LOAD_FUNC(SQLDescribeColW);
    LOAD_FUNC(SQLDescribeParam);
    LOAD_FUNC(SQLDisconnect);
    LOAD_FUNC(SQLDriverConnect);
    LOAD_FUNC(SQLDriverConnectW);
    LOAD_FUNC(SQLDrivers);
    LOAD_FUNC(SQLDriversW);
    LOAD_FUNC(SQLEndTran);
    LOAD_FUNC(SQLError);
    LOAD_FUNC(SQLErrorW);
    LOAD_FUNC(SQLExecDirect);
    LOAD_FUNC(SQLExecDirectW);
    LOAD_FUNC(SQLExecute);
    LOAD_FUNC(SQLExtendedFetch);
    LOAD_FUNC(SQLFetch);
    LOAD_FUNC(SQLFetchScroll);
    LOAD_FUNC(SQLForeignKeys);
    LOAD_FUNC(SQLForeignKeysW);
    LOAD_FUNC(SQLFreeConnect);
    LOAD_FUNC(SQLFreeEnv);
    LOAD_FUNC(SQLFreeHandle);
    LOAD_FUNC(SQLFreeStmt);
    LOAD_FUNC(SQLGetConnectAttr);
    LOAD_FUNC(SQLGetConnectAttrW);
    LOAD_FUNC(SQLGetConnectOption);
    LOAD_FUNC(SQLGetConnectOptionW);
    LOAD_FUNC(SQLGetCursorName);
    LOAD_FUNC(SQLGetCursorNameW);
    LOAD_FUNC(SQLGetData);
    LOAD_FUNC(SQLGetDescField);
    LOAD_FUNC(SQLGetDescFieldW);
    LOAD_FUNC(SQLGetDescRec);
    LOAD_FUNC(SQLGetDescRecW);
    LOAD_FUNC(SQLGetDiagField);
    LOAD_FUNC(SQLGetDiagFieldW);
    LOAD_FUNC(SQLGetDiagRec);
    LOAD_FUNC(SQLGetDiagRecA);
    LOAD_FUNC(SQLGetDiagRecW);
    LOAD_FUNC(SQLGetEnvAttr);
    LOAD_FUNC(SQLGetFunctions);
    LOAD_FUNC(SQLGetInfo);
    LOAD_FUNC(SQLGetInfoW);
    LOAD_FUNC(SQLGetStmtAttr);
    LOAD_FUNC(SQLGetStmtAttrW);
    LOAD_FUNC(SQLGetStmtOption);
    LOAD_FUNC(SQLGetTypeInfo);
    LOAD_FUNC(SQLGetTypeInfoW);
    LOAD_FUNC(SQLMoreResults);
    LOAD_FUNC(SQLNativeSql);
    LOAD_FUNC(SQLNativeSqlW);
    LOAD_FUNC(SQLNumParams);
    LOAD_FUNC(SQLNumResultCols);
    LOAD_FUNC(SQLParamData);
    LOAD_FUNC(SQLParamOptions);
    LOAD_FUNC(SQLPrepare);
    LOAD_FUNC(SQLPrepareW);
    LOAD_FUNC(SQLPrimaryKeys);
    LOAD_FUNC(SQLPrimaryKeysW);
    LOAD_FUNC(SQLProcedureColumns);
    LOAD_FUNC(SQLProcedureColumnsW);
    LOAD_FUNC(SQLProcedures);
    LOAD_FUNC(SQLProceduresW);
    LOAD_FUNC(SQLPutData);
    LOAD_FUNC(SQLRowCount);
    LOAD_FUNC(SQLSetConnectAttr);
    LOAD_FUNC(SQLSetConnectAttrW);
    LOAD_FUNC(SQLSetConnectOption);
    LOAD_FUNC(SQLSetConnectOptionW);
    LOAD_FUNC(SQLSetCursorName);
    LOAD_FUNC(SQLSetCursorNameW);
    LOAD_FUNC(SQLSetDescField);
    LOAD_FUNC(SQLSetDescFieldW);
    LOAD_FUNC(SQLSetDescRec);
    LOAD_FUNC(SQLSetEnvAttr);
    LOAD_FUNC(SQLSetParam);
    LOAD_FUNC(SQLSetPos);
    LOAD_FUNC(SQLSetScrollOptions);
    LOAD_FUNC(SQLSetStmtAttr);
    LOAD_FUNC(SQLSetStmtAttrW);
    LOAD_FUNC(SQLSetStmtOption);
    LOAD_FUNC(SQLSpecialColumns);
    LOAD_FUNC(SQLSpecialColumnsW);
    LOAD_FUNC(SQLStatistics);
    LOAD_FUNC(SQLStatisticsW);
    LOAD_FUNC(SQLTablePrivileges);
    LOAD_FUNC(SQLTablePrivilegesW);
    LOAD_FUNC(SQLTables);
    LOAD_FUNC(SQLTablesW);
    LOAD_FUNC(SQLTransact);
#undef LOAD_FUNC
    return STATUS_SUCCESS;
}
