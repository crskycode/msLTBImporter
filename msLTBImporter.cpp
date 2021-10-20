// msLTBImporter.cpp : Defines the initialization routines for the DLL.
//

#include <windows.h>
#include "msLTBImporter.h"
#include "de_file.h"
#include "model.h"
#include "LTEulerAngles.h"


//
//TODO: If this DLL is dynamically linked against the MFC DLLs,
//		any functions exported from this DLL which call into
//		MFC must have the AFX_MANAGE_STATE macro added at the
//		very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//



void *DefStdlithAlloc(uint32 size)
{
	return calloc(1, size);
}

void DefStdlithFree(void *ptr)
{
	free(ptr);
}


void dsi_PrintToConsole(const char *pMsg, ...)
{
	//va_list ptr;
	//static char string[1024];
	//
	//va_start(ptr, pMsg);
	//vsnprintf(string, sizeof(string), pMsg, ptr);
	//va_end(ptr);

	//OutputDebugString(string);
}

void dsi_OnReturnError(int err)
{
}


cMsPlugIn *CreatePlugIn()
{
	return new cPlugIn();
}


cPlugIn::cPlugIn()
{
	strcpy(szTitle, "LithTech LTB...");
}

cPlugIn::~cPlugIn()
{
}

int cPlugIn::GetType(void)
{
	return cMsPlugIn::eTypeImport;
}

const char *cPlugIn::GetTitle(void)
{
	return szTitle;
}

#define CONVERT_TO_RIGHT_HAND

static int boneindextable[256];
static int boneindex;

void RecursiveSetUpBone(msModel *pModel, Model *ltbModel, ModelNode *pNode, const char *pParentName)
{
	int nBone = msModel_AddBone(pModel);
	msBone *pBone = msModel_GetBoneAt(pModel, nBone);

	boneindextable[boneindex++] = nBone;

	msBone_SetName(pBone, pNode->GetName());

	if (pParentName)
		msBone_SetParentName(pBone, pParentName);

	LTMatrix mBoneMatrix;
	LTVector vPos;
	EulerAngles vEul;

	mBoneMatrix = pNode->GetFromParentTransform();

#ifdef CONVERT_TO_RIGHT_HAND
	// Convert ratation to RH
	mBoneMatrix.m[0][2] = -mBoneMatrix.m[0][2];
	mBoneMatrix.m[1][2] = -mBoneMatrix.m[1][2];
	mBoneMatrix.m[2][0] = -mBoneMatrix.m[2][0];
	mBoneMatrix.m[2][1] = -mBoneMatrix.m[2][1];
	// Convert translation to RH
	mBoneMatrix.m[2][3] = -mBoneMatrix.m[2][3];
#endif

	vEul = Eul_FromMatrix(mBoneMatrix, EulOrdXYZs);
	mBoneMatrix.GetTranslation(vPos);

	msVec3 pos = { vPos.x, vPos.y, vPos.z };
	msVec3 rot = { vEul.x, vEul.y, vEul.z };
	msBone_SetPosition( pBone, pos );
	msBone_SetRotation( pBone, rot );

	for (uint32 i = 0; i < pNode->m_Children.GetSize(); i++)
	{
		RecursiveSetUpBone(pModel, ltbModel, pNode->m_Children[i], pNode->GetName());
	}
}

int cPlugIn::Execute(msModel *pModel)
{
	if (!pModel)
		return -1;

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));

	char szFile[MS_MAX_PATH];
	char szFileTitle[MS_MAX_PATH];
	char szDefExt[32] = "ltb";
	char szFilter[128] = "LithTech LTB Files (*.ltb)\0*.ltb\0All Files (*.*)\0*.*\0\0";
	szFile[0] = '\0';
	szFileTitle[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrDefExt = szDefExt;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MS_MAX_PATH;
	ofn.lpstrFileTitle = szFileTitle;
	ofn.nMaxFileTitle = MS_MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	ofn.lpstrTitle = "Import LithTech LTB";

	if (!::GetOpenFileName(&ofn))
		return 0;

	DosFileStream *pStream = new DosFileStream();
	if (!pStream)
		return 0;

	if (pStream->Open(szFile) != LT_OK)
	{
		delete pStream;
		::MessageBox(GetActiveWindow(), "Couldn't to open file", "Error", MB_ICONERROR | MB_OK);
		return 0;
	}

	ModelLoadRequest *pRequest = new ModelLoadRequest();
	if (!pRequest)
	{
		pStream->Close();
		delete pStream;
		::MessageBox(GetActiveWindow(), "Out of memory", "Error", MB_ICONERROR | MB_OK);
		return 0;
	}

	memset(pRequest, 0, sizeof(ModelLoadRequest));
	pRequest->m_pFile = pStream;
	pRequest->m_pFilename = szFile;

	Model *pInpModel = new Model();
	if (!pInpModel)
	{
		pStream->Close();
		delete pStream;
		delete pRequest;
		::MessageBox(GetActiveWindow(), "Out of memory", "Error", MB_ICONERROR | MB_OK);
		return 0;
	}

	if (pInpModel->Load(pRequest, szFile) != LT_OK)
	{
		pStream->Close();
		delete pStream;
		delete pRequest;
		delete pInpModel;
		::MessageBox(GetActiveWindow(), "Couldn't to load file", "Error", MB_ICONERROR | MB_OK);
		return 0;
	}

	//Destroy the bones
//	if (msModel_GetBoneCount() > 0)
//	{
//	}

	boneindex = 0;

	//Node <--> Bone
	RecursiveSetUpBone(pModel, pInpModel, pInpModel->GetRootNode(), NULL);

#ifdef CONVERT_TO_RIGHT_HAND
	LTMatrix mMat1;
	mMat1.SetupScalingMatrix(LTVector(1.0, 1.0, -1.0));
#endif

	//Pieces <--> Mesh
	for (uint32 i = 0; i < pInpModel->m_Pieces.GetSize(); i++)
	{
		ModelPiece *pPiece = pInpModel->m_Pieces[i];

		CDIModelDrawable *pLOD = pPiece->GetLOD(0);
		if ( !pLOD )
			continue;

		//Add Group
		int nMesh = msModel_AddMesh(pModel);
		msMesh *pMesh = msModel_GetMeshAt(pModel, nMesh);

		//Set Name
		msMesh_SetName(pMesh, pPiece->GetName());

		for (uint32 j = 0; j < pLOD->m_Verts.GetSize(); j++)
		{
			//Add Vertex
			int nVertex = msMesh_AddVertex(pMesh);
			msVertex* pVertex = msMesh_GetVertexAt(pMesh, nVertex);
			msVertexEx* pVertexEx = msMesh_GetVertexExAt(pMesh, nVertex);

			//Add Normal
			int nVertexNorm = msMesh_AddVertexNormal(pMesh);

			//Get Vertex
			ModelVert * pvert = &pLOD->m_Verts[j];

			LTVector Vec;
			LTVector Normal;

#ifdef CONVERT_TO_RIGHT_HAND
			mMat1.Apply4x4(pvert->m_Vec, Vec);
			mMat1.Apply3x3(pvert->m_Normal, Normal);
#endif

			msVec3 pos = { Vec.x, Vec.y, Vec.z };
			msVec3 norm = { Normal.x, Normal.y, Normal.z };
			msVec2 st = { pvert->m_Uv.tu, pvert->m_Uv.tv };

			//SetUp Vertex
			msVertex_SetVertex(pVertex, pos);
			msVertex_SetTexCoords(pVertex, st);

			if (pvert->m_NumBones == 1)
			{
				msVertex_SetBoneIndex(pVertex, boneindextable[pvert->m_Weights.m_iBone[0]]);
			}
			else if (pvert->m_NumBones > 1)
			{
				// WHAT THE FUCK ???

				msVertex_SetBoneIndex(pVertex, boneindextable[pvert->m_Weights.m_iBone[0]]);
				for (uint32 q = 1; q < pvert->m_NumBones; q++)
					msVertexEx_SetBoneIndices(pVertexEx, q - 1, boneindextable[pvert->m_Weights.m_iBone[q]]);

				uint32 num = min(3, pvert->m_NumBones);
				for (uint32 q = 0; q < num; q++)
					msVertexEx_SetBoneWeights(pVertexEx, q, pvert->m_Weights.m_fWeight[q] * 100.0f);
			}

			//SetUp Normal
			msMesh_SetVertexNormalAt(pMesh, nVertexNorm, norm);
		}

		for (uint32 j = 0; j < pLOD->m_Tris.GetSize(); j++)
		{
			//Add Triangle
			int nTriangle = msMesh_AddTriangle(pMesh);
			msTriangle *pTriangle = msMesh_GetTriangleAt(pMesh, nTriangle);

			//Get Triangle
			ModelTri *TRI = &pLOD->m_Tris[j];

#ifdef CONVERT_TO_RIGHT_HAND
			word nIndices[3] = { TRI->m_Indices[2], TRI->m_Indices[1], TRI->m_Indices[0] };
#else
			word nIndices[3] = { TRI->m_Indices[0], TRI->m_Indices[1], TRI->m_Indices[2] };
#endif

			//SetUp Triangle
			msTriangle_SetVertexIndices(pTriangle, nIndices);
			msTriangle_SetNormalIndices(pTriangle, nIndices);
		}
	}

	pStream->Close();
	delete pStream;
	delete pRequest;
	delete pInpModel;
	return 0;
}

BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpReserved)  // reserved
{
	// Perform actions based on the reason for calling.
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		// Initialize once for each new process.
		// Return FALSE to fail DLL load.
		break;

	case DLL_THREAD_ATTACH:
		// Do thread-specific initialization.
		break;

	case DLL_THREAD_DETACH:
		// Do thread-specific cleanup.
		break;

	case DLL_PROCESS_DETACH:
		// Perform any necessary cleanup.
		break;
	}
	return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
