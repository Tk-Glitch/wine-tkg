@ stub IsVolumeSnapshotted
@ stub VssFreeSnapshotProperties
@ stub ShouldBlockRevert
@ stub ??0CVssJetWriter@@QAE@XZ
@ thiscall -arch=i386 ??0CVssWriter@@QAE@XZ(ptr) VSSAPI_CVssWriter_default_ctor
@ cdecl -arch=win64 ??0CVssWriter@@QEAA@XZ(ptr) VSSAPI_CVssWriter_default_ctor
@ stub ??1CVssJetWriter@@UAE@XZ
@ thiscall -arch=i386 ??1CVssWriter@@UAE@XZ(ptr) VSSAPI_CVssWriter_dtor
@ cdecl -arch=win64 ??1CVssWriter@@UEAA@XZ(ptr) VSSAPI_CVssWriter_dtor
@ stub ?AreComponentsSelected@CVssJetWriter@@IBG_NXZ
@ stub ?AreComponentsSelected@CVssWriter@@IBG_NXZ
@ stdcall ?CreateVssBackupComponents@@YGJPAPAVIVssBackupComponents@@@Z(ptr) VSSAPI_CreateVssBackupComponents
@ stub ?CreateVssExamineWriterMetadata@@YGJPAGPAPAVIVssExamineWriterMetadata@@@Z
@ stub ?CreateVssSnapshotSetDescription@@YGJU_GUID@@JPAPAVIVssSnapshotSetDescription@@@Z
@ stub ?GetBackupType@CVssJetWriter@@IBG?AW4_VSS_BACKUP_TYPE@@XZ
@ stub ?GetBackupType@CVssWriter@@IBG?AW4_VSS_BACKUP_TYPE@@XZ
@ stub ?GetContext@CVssJetWriter@@IBGJXZ
@ stub ?GetContext@CVssWriter@@IBGJXZ
@ stub ?GetCurrentLevel@CVssJetWriter@@IBG?AW4_VSS_APPLICATION_LEVEL@@XZ
@ stub ?GetCurrentLevel@CVssWriter@@IBG?AW4_VSS_APPLICATION_LEVEL@@XZ
@ stub ?GetCurrentSnapshotSetId@CVssJetWriter@@IBG?AU_GUID@@XZ
@ stub ?GetCurrentSnapshotSetId@CVssWriter@@IBG?AU_GUID@@XZ
@ stub ?GetCurrentVolumeArray@CVssJetWriter@@IBGPAPBGXZ
@ stub ?GetCurrentVolumeArray@CVssWriter@@IBGPAPBGXZ
@ stub ?GetCurrentVolumeCount@CVssJetWriter@@IBGIXZ
@ stub ?GetCurrentVolumeCount@CVssWriter@@IBGIXZ
@ stub ?GetRestoreType@CVssJetWriter@@IBG?AW4_VSS_RESTORE_TYPE@@XZ
@ stub ?GetRestoreType@CVssWriter@@IBG?AW4_VSS_RESTORE_TYPE@@XZ
@ stub ?GetSnapshotDeviceName@CVssJetWriter@@IBGJPBGPAPBG@Z
@ stub ?GetSnapshotDeviceName@CVssWriter@@IBGJPBGPAPBG@Z
@ stub ?Initialize@CVssJetWriter@@QAGJU_GUID@@PBG_N211K@Z
@ thiscall -arch=i386 ?Initialize@CVssWriter@@QAGJU_GUID@@PBGW4VSS_USAGE_TYPE@@W4VSS_SOURCE_TYPE@@W4_VSS_APPLICATION_LEVEL@@KW4VSS_ALTERNATE_WRITER_STATE@@_N@Z(ptr int128 wstr long long long long long long wstr) VSSAPI_CVssWriter_Initialize
@ cdecl -arch=win64 ?Initialize@CVssWriter@@QEAAJU_GUID@@PEBGW4VSS_USAGE_TYPE@@W4VSS_SOURCE_TYPE@@W4_VSS_APPLICATION_LEVEL@@KW4VSS_ALTERNATE_WRITER_STATE@@_N1@Z(ptr int128 wstr long long long long long long wstr) VSSAPI_CVssWriter_Initialize
@ stub ?InstallAlternateWriter@CVssWriter@@QAGJU_GUID@@0@Z
@ stub ?IsBootableSystemStateBackedUp@CVssJetWriter@@IBG_NXZ
@ stub ?IsBootableSystemStateBackedUp@CVssWriter@@IBG_NXZ
@ stub ?IsPartialFileSupportEnabled@CVssJetWriter@@IBG_NXZ
@ stub ?IsPartialFileSupportEnabled@CVssWriter@@IBG_NXZ
@ stub ?IsPathAffected@CVssJetWriter@@IBG_NPBG@Z
@ stub ?IsPathAffected@CVssWriter@@IBG_NPBG@Z
@ stub ?LoadVssSnapshotSetDescription@@YGJPBGPAPAVIVssSnapshotSetDescription@@U_GUID@@@Z
@ stub ?OnAbortBegin@CVssJetWriter@@UAGXXZ
@ stub ?OnAbortEnd@CVssJetWriter@@UAGXXZ
@ stub ?OnBackOffIOOnVolume@CVssWriter@@UAG_NPAGU_GUID@@1@Z
@ stub ?OnBackupComplete@CVssWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnBackupCompleteBegin@CVssJetWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnBackupCompleteEnd@CVssJetWriter@@UAG_NPAVIVssWriterComponents@@_N@Z
@ stub ?OnBackupShutdown@CVssWriter@@UAG_NU_GUID@@@Z
@ stub ?OnContinueIOOnVolume@CVssWriter@@UAG_NPAGU_GUID@@1@Z
@ stub ?OnFreezeBegin@CVssJetWriter@@UAG_NXZ
@ stub ?OnFreezeEnd@CVssJetWriter@@UAG_N_N@Z
@ stub ?OnIdentify@CVssJetWriter@@UAG_NPAVIVssCreateWriterMetadata@@@Z
@ stub ?OnIdentify@CVssWriter@@UAG_NPAVIVssCreateWriterMetadata@@@Z
@ stub ?OnPostRestore@CVssWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnPostRestoreBegin@CVssJetWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnPostRestoreEnd@CVssJetWriter@@UAG_NPAVIVssWriterComponents@@_N@Z
@ stub ?OnPostSnapshot@CVssJetWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnPostSnapshot@CVssWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnPreRestore@CVssWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnPreRestoreBegin@CVssJetWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnPreRestoreEnd@CVssJetWriter@@UAG_NPAVIVssWriterComponents@@_N@Z
@ stub ?OnPrepareBackup@CVssWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnPrepareBackupBegin@CVssJetWriter@@UAG_NPAVIVssWriterComponents@@@Z
@ stub ?OnPrepareBackupEnd@CVssJetWriter@@UAG_NPAVIVssWriterComponents@@_N@Z
@ stub ?OnPrepareSnapshotBegin@CVssJetWriter@@UAG_NXZ
@ stub ?OnPrepareSnapshotEnd@CVssJetWriter@@UAG_N_N@Z
@ stub ?OnThawBegin@CVssJetWriter@@UAG_NXZ
@ stub ?OnThawEnd@CVssJetWriter@@UAG_N_N@Z
@ stub ?OnVSSApplicationStartup@CVssWriter@@UAG_NXZ
@ stub ?OnVSSShutdown@CVssWriter@@UAG_NXZ
@ stub ?SetWriterFailure@CVssJetWriter@@IAGJJ@Z
@ stub ?SetWriterFailure@CVssWriter@@IAGJJ@Z
@ thiscall -arch=i386 ?Subscribe@CVssWriter@@QAGJK@Z(ptr long) VSSAPI_CVssWriter_Subscribe
@ cdecl -arch=win64 ?Subscribe@CVssWriter@@QEAAJK@Z(ptr long) VSSAPI_CVssWriter_Subscribe
@ stub ?Uninitialize@CVssJetWriter@@QAGXXZ
@ thiscall -arch=i386 ?Unsubscribe@CVssWriter@@QAGJXZ(ptr) VSSAPI_CVssWriter_Unsubscribe
@ cdecl -arch=win64 ?Unsubscribe@CVssWriter@@QEAAJXZ(ptr) VSSAPI_CVssWriter_Unsubscribe
@ stdcall CreateVssBackupComponentsInternal(ptr)
@ stub CreateVssExamineWriterMetadataInternal
@ stub CreateVssExpressWriterInternal
@ stub CreateWriter
@ stub CreateWriterEx
@ stub DllCanUnloadNow
@ stub DllGetClassObject
@ stub GetProviderMgmtInterface
@ stub GetProviderMgmtInterfaceInternal
@ stub IsVolumeSnapshottedInternal
@ stub ShouldBlockRevertInternal
@ stub VssFreeSnapshotPropertiesInternal
