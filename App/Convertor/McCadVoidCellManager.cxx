#include "McCadVoidCellManager.hxx"
#include "McCadMcnpWriter.hxx"
#include "McCadTripoliWriter.hxx"

#include <assert.h>
#include <TCollection_AsciiString.hxx>
#include <OSD_Path.hxx>
#include <OSD_Protection.hxx>
#include <QStringList>

#include "../Tools/McCadMathTool.hxx"
#include "../Tools/McCadConvertConfig.hxx"
#include "../Tools/McCadGeneTool.hxx"

McCadVoidCellManager::McCadVoidCellManager()
{
    m_bGenerateVoid = McCadConvertConfig::GenerateVoid();   // Turn on/off the void generating switch
    if (m_bGenerateVoid == 0)
    {
        McCadConvertConfig::SetMinMaxSmplPntNum(10,20);
    }
    m_Convertor = "MCNP"; // The default convetor is MCNP

    msg = McCadMessager_Signal::Instance();
}

McCadVoidCellManager::~McCadVoidCellManager()
{
}


/** ********************************************************************
* @brief Set the output file name according to the input file name
*
* @param TCollection_AsciiString theFileName
* @return void
*
* @date 07/01/2014
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::SetOutFileName(TCollection_AsciiString theFileName)
{
    m_OutFileName = theFileName;
}



/** ********************************************************************
* @brief The main process control function
*        1. call the void generation and collision detect function
*        2. Load the material information
*        3. Output the input file
*
* @param null
* @return void
*
* @date 31/10/2012
* @modify 18/12/2013
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::Process()
{
    // Add by Lei 12/11/2013 Load the material XML file.
    //TCollection_AsciiString material_file = "material.xml";
    //ReadMatData(material_file);

    if(this->GenVoid())
    {
        McCadVoidGenerator *pVoidGen = new McCadVoidGenerator(this);
        pVoidGen->Process();

        delete pVoidGen;
        pVoidGen = NULL;
    }

    m_pGeomData->SortSurface();
    m_pGeomData->UpdateFaceNum();

    IMcCadWriter * pWriter = NULL; 
    if (m_Convertor == "TRIPOLI")
    {
        pWriter = new McCadTripoliWriter();
        ((McCadTripoliWriter *)pWriter)->SetVirtCellNum(10000);
    }
    else if (m_Convertor == "MCNP")
    {
        pWriter = new McCadMcnpWriter();
    }

    if (pWriter != NULL)
    {
        pWriter->SetMaterial(m_bHaveMaterial);
        pWriter->SetVoid(m_bGenerateVoid);
        pWriter->SetManager(this);
        pWriter->SetFileName(m_OutFileName);         
        pWriter->PrintFile();

        delete pWriter;
        pWriter = NULL;
    }

    //cout<<endl;
    //cout<<"o =============== Finish conversion =============== o"<<endl;

    PrintInfo("o =============== Finish conversion =============== o",true);
}



/** ********************************************************************
* @brief Read the geometry data, generate the solid, convexsolid and
*        surface list and store them into the "McCadGeomData" object.
* @param Handle_TopTools_HSequenceOfShape & solid_list:
*        The Input solid list
* @return void
*
* @date 31/10/2012
* @modify 18/12/2013
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::ReadGeomData(Handle_TopTools_HSequenceOfShape & solid_list)
{
    PrintInfo("============== Step.1  Read geometry data ============",true);

    McCadGeomData *pData = new McCadGeomData(solid_list,m_bGenerateVoid);
    m_pGeomData.reset(pData);
    m_pGeomData->SetManager(this);
}


/** ********************************************************************
* @brief Read the material data
* @param TCollection_AsciiString & theMatFile:  Material XML file
* @return void
*
* @date 31/10/2012
* @modify 18/12/2013
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::ReadMatData(TCollection_AsciiString &theMatFile)
{    
    PrintInfo("============== Step.2  Load material data ============",true);

    if(theMatFile.IsEmpty())
    {
        msg->sig_message_append(QString::fromLatin1(">>> No Material.xml file."));
        return;
    }

    MaterialManager* pMatManager = new MaterialManager();
    Standard_Boolean bSetMaterial = pMatManager->LoadXMLfile(QString(theMatFile.ToCString()),true);
    if (bSetMaterial)
    {
        m_pMat.reset(pMatManager);
        AddMatOnSolid();
        m_bHaveMaterial = Standard_True;
        msg->sig_message_append(QString::fromLatin1(">>> Material.xml has been loaded..."));
    }
    else
    {
        m_bHaveMaterial = Standard_False;
    }
}



/** ********************************************************************
* @brief Return the geometry data pointer
* @param
* @return McCadGeomData * : the pointer of geometry pointer
*
* @date 31/10/2012
* @modify 18/12/2012
* @author  Lei Lu
***********************************************************************/
McCadGeomData * McCadVoidCellManager::GetGeomData()
{
    return m_pGeomData.get();
}



/** ********************************************************************
* @brief Return the material data pointer
* @param
* @return MaterialManager * : the pointer of material manager
*
* @date 31/10/2012
* @modify 18/12/2012
* @author  Lei Lu
***********************************************************************/
MaterialManager * McCadVoidCellManager::GetMatManager()
{
    return m_pMat.get();
}



/** ********************************************************************
* @brief Return the material data pointer
* @param
* @return MaterialManager * : the pointer of material manager
*
* @date 18/12/2012
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::SetConvetor(TCollection_AsciiString theConvertor)
{
    m_Convertor = theConvertor;
}



/** ********************************************************************
* @brief The void generation switch
* @param
* @return Standard_Boolean: Generate the void or not
*
* @date 18/12/2013
* @author  Lei Lu
***********************************************************************/
Standard_Boolean McCadVoidCellManager::GenVoid() const
{
    return m_bGenerateVoid;
}


/** ********************************************************************
* @brief Connect the materials with the solids. Add the material number
*        and density to each solid.
* @param
* @return
*
* @date 31/10/2012
* @modify 18/12/2012
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::AddMatOnSolid()
{
    m_bHaveMaterial = Standard_True;    // The reading of material information is successful

    McCadSolid * pSolid = NULL;
    for (unsigned int i = 0; i < m_pGeomData->m_SolidList.size(); i++)
    {
        pSolid = m_pGeomData->m_SolidList.at(i);

        QString groupName = m_pMat->GetGroupName(i+1);        // Get group name according to solid id
        int iMatID = m_pMat->GetMatNum(groupName);            // Get material id according to group name
        double fDensity = m_pMat->GetMatDensity(iMatID);      // Get material density according to material id
        QString material_name = m_pMat->GetMatName(iMatID);   // Get material name according to material id

        pSolid->SetMaterial(iMatID,fDensity);
        pSolid->SetName(McCadGeneTool::ToAsciiString(material_name));
    }
}


/** ********************************************************************
* @brief  Print the information and message during the conversion
*
* @param TCollection_AsciiString & info
* @return
*
* @date 20/08/2018
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::PrintInfo(const char *info, bool opt)
{
    if (opt) // opt: print line at the new line
    {
        msg->sig_message_append(QString::fromLatin1(info));
    }
    else     // append the text
    {
        msg->sig_message_insert(QString::fromLatin1(info));
    }
}



/** ********************************************************************
* @brief  Set the main progress bar
*
* @param float fStep
* @return
*
* @date 20/08/2018
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::SetMainProgressBar(int fStep)
{
    msg->sig_progress_main(fStep);
}



/** ********************************************************************
* @brief  Set the part progress bar
*
* @param float fStep
* @return
*
* @date 20/08/2018
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::SetPartProgressBar(int fStep)
{
    msg->sig_progress_part(fStep);
}



/** ********************************************************************
* @brief  Print the information and message during the conversion
*
* @param TCollection_AsciiString & info
* @return
*
* @date 20/08/2018
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::SetPartLabel(const char *info)
{
     msg->sig_part_label(QString::fromLatin1(info));
}



/** ********************************************************************
* @brief  Print the information and message during the conversion
*
* @param TCollection_AsciiString & info
* @return
*
* @date 20/08/2018
* @author  Lei Lu
***********************************************************************/
void McCadVoidCellManager::SetMainLabel(const char *info)
{
     msg->sig_main_label(QString::fromLatin1(info));
}


