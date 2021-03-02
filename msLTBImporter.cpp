// msLTBImporter.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "msLTBImporter.h"
#include "de_file.h"
#include "model.h"
#include "LTEulerAngles.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/type_ptr.hpp>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

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


// CmsLTBImporterApp

BEGIN_MESSAGE_MAP(CMsPlugInApp, CWinApp)
END_MESSAGE_MAP()


// CmsLTBImporterApp construction

CMsPlugInApp::CMsPlugInApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CmsLTBImporterApp object

CMsPlugInApp theApp;


// CmsLTBImporterApp initialization

BOOL CMsPlugInApp::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}


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
	va_list ptr;
	static char string[1024];
	
	va_start(ptr, pMsg);
	vsnprintf(string, sizeof(string), pMsg, ptr);
	va_end(ptr);

	OutputDebugString(string);
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

typedef float mat4_t[4][4];

typedef float (*pmat4_t)[4];

#define glm_ptr( x ) glm::value_ptr( x )

void MatrixCopy( const mat4_t a, mat4_t b )
{
	int	i, j;

	for ( i = 0; i < 4; i++ )
	{
		for ( j = 0; j < 4; j++ )
		{
			b[j][i] = a[i][j];
		}
	}
}

// left-handed to right-handed
glm::mat4 mLeftToRight = glm::scale( glm::mat4( 1.0f ), glm::vec3( 1.0f, 1.0f, -1.0f ) );


void RecursiveSetUpBone(msModel *pModel, Model *ltbModel, ModelNode *pNode, const char *pParentName)
{
	int nBone = msModel_AddBone(pModel);
	msBone *pBone = msModel_GetBoneAt(pModel, nBone);

	msBone_SetName(pBone, pNode->GetName());

	if (pParentName)
		msBone_SetParentName(pBone, pParentName);

	glm::mat4 mParentL( 1.0f );

	if ( pNode->m_iParentNode != NODEPARENT_NONE )
	{
		LTMatrix ltParent;
		ltParent = ltbModel->GetNode( pNode->m_iParentNode )->GetGlobalTransform();
		MatrixCopy( ltParent.m, (pmat4_t)glm_ptr( mParentL ) );
	}

	// Convert to Right-Hand
	glm::mat4 mParentR;
	mParentR = /*mLeftToRight **/ mParentL;

	LTMatrix ltGlobal;
	ltGlobal = pNode->GetGlobalTransform();

	glm::mat4 mGlobalL;
	MatrixCopy( ltGlobal.m, (pmat4_t)glm_ptr( mGlobalL ) );

	// Convert to Right-Hand
	glm::mat4 mGlobalR;
	mGlobalR = /*mLeftToRight **/ mGlobalL;

	glm::mat4 mLocal;
	mLocal = glm::inverse( mParentR ) * mGlobalR;

	// Convert matrix to quaternion
	glm::quat qRot;
	qRot = glm::quat_cast( mLocal );

	// Convert quaternion to euler angles
	glm::vec3 eRot;
	eRot = glm::eulerAngles( qRot );

	msVec3 pos = { mLocal[3][0], mLocal[3][1], mLocal[3][2] };
	msVec3 rot = { eRot.x, eRot.y, eRot.z };
	msBone_SetPosition( pBone, pos );
	msBone_SetRotation( pBone, rot );

	for (uint32 i = 0; i < pNode->m_Children.GetSize(); i++)
	{
		RecursiveSetUpBone(pModel, ltbModel, pNode->m_Children[i], pNode->GetName());
	}
}

int ChooseWeights(Weights &w)
{
	//Choose the first bone
	for (int i = 0; i < 4; ++i)
	{
		if (w.m_fWeight[i] > 0.0f)
			return w.m_iBone[i];
	}

	//If all weights are zero, should choose the last one
	for (int i = 3; i > -1; --i)
	{
		if (w.m_iBone[i] != 0 && w.m_iBone[i] != 255)
			return w.m_iBone[i];
	}

	//Invalid weights ?
	return 0;
}

int cPlugIn::Execute(msModel *pModel)
{
	if (!pModel)
		return -1;

	//Switch the module state for MFC Dlls
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

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
		::AfxMessageBox("Couldn't to open file", MB_ICONERROR | MB_OK);
		return 0;
	}

	ModelLoadRequest *pRequest = new ModelLoadRequest();
	if (!pRequest)
	{
		pStream->Close();
		delete pStream;
		::AfxMessageBox("Out of memory", MB_ICONERROR | MB_OK);
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
		::AfxMessageBox("Out of memory", MB_ICONERROR | MB_OK);
		return 0;
	}

	if (pInpModel->Load(pRequest, szFile) != LT_OK)
	{
		pStream->Close();
		delete pStream;
		delete pRequest;
		delete pInpModel;
		::AfxMessageBox("Couldn't to load file", MB_ICONERROR | MB_OK);
		return 0;
	}

	//Destroy the bones
//	if (msModel_GetBoneCount() > 0)
//	{
//	}
	//Node <--> Bone
	RecursiveSetUpBone(pModel, pInpModel, pInpModel->GetRootNode(), NULL);

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
			msVertex *pVertex = msMesh_GetVertexAt(pMesh, nVertex);

			//Add Normal
			int nVertexNorm = msMesh_AddVertexNormal(pMesh);

			//Get Vertex
			ModelVert *VERT = &pLOD->m_Verts[j];

			glm::vec4 lPos;
			lPos.x = VERT->m_Vec.x;
			lPos.y = VERT->m_Vec.y;
			lPos.z = VERT->m_Vec.z;
			lPos.w = 1.0f;

			glm::vec4 lNor;
			lNor.x = VERT->m_Normal.x;
			lNor.y = VERT->m_Normal.y;
			lNor.z = VERT->m_Normal.z;
			lNor.w = 0.0f;

			glm::vec4 rPos;
			rPos = mLeftToRight * lPos;

			glm::vec4 rNor;
			rNor = mLeftToRight * lNor;

			msVec3 pos = { rPos.x, rPos.y, rPos.z };
			msVec3 norm = { rNor.x, rNor.y, rNor.z };
			msVec2 st = { VERT->m_Uv.tu, VERT->m_Uv.tv };

		//	int nBone = ChooseWeights(VERT->m_Weights);

			//SetUp Vertex
			msVertex_SetVertex(pVertex, pos);
			msVertex_SetTexCoords(pVertex, st);
		//	msVertex_SetBoneIndex(pVertex, nBone);

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

			//word nIndices[3] = { TRI->m_Indices[0], TRI->m_Indices[1], TRI->m_Indices[2] };
			word nIndices[3] = { TRI->m_Indices[2], TRI->m_Indices[1], TRI->m_Indices[0] };

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
