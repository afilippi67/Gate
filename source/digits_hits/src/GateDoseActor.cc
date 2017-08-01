/*----------------------
  Copyright (C): OpenGATE Collaboration

  This software is distributed under the terms
  of the GNU Lesser General  Public Licence (LGPL)
  See GATE/LICENSE.txt for further details
  ----------------------*/

/*
  \brief Class GateDoseActor :
  \brief
*/

// gate
#include "GateDoseActor.hh"
#include "GateMiscFunctions.hh"

// g4
#include <G4EmCalculator.hh>
#include <G4VoxelLimits.hh>
#include <G4NistManager.hh>
#include <G4PhysicalConstants.hh>
#include <G4Gamma.hh>
#include <G4Proton.hh>
#include <G4Positron.hh>
#include <G4Deuteron.hh>
#include <G4Electron.hh>

#include "G4MaterialTable.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4ProcessManager.hh"

//------------------------------------------------------------------------------------------------------
//  try get N values of type T from a given input line
// * throw exception with informative error message in case of trouble.
// * NOTE that while this catches some common errors, it is not yet fool proof.
template<typename T, int N>
typename std::vector<T> parse_N_values_of_type_T(std::string line,int lineno, const std::string& fname){
  GateMessage("Beam", 5, "[DoseActor] trying to parse line " << lineno << " from file " << fname << Gateendl );
  std::istringstream iss(line);
  typename std::istream_iterator<T> iss_end;
  typename std::istream_iterator<T> isiT(iss);
  typename std::vector<T> vecT;
  while (isiT != iss_end) vecT.push_back(*(isiT++));
  int nread = vecT.size();
  if (nread != N){
    std::ostringstream errMsg;
    errMsg << "wrong number of values (" << nread << ") on line " << lineno << " of " << fname
           << ", expected " << N << " value(s) of type " << typeid(T).name() << std::endl;
    throw std::runtime_error(errMsg.str());
  }
  return vecT;
}
//------------------------------------------------------------------------------------------------------
// Function to read the next content line
// * skip all comment lines (lines string with a '#')
// * skip empty
// * throw exception with informative error message in case of missing data
std::string ReadNextLine( std::istream& input, int& lineno, const std::string& fname ) {
  while ( input ){
    std::string line;
    std::getline(input,line);
    ++lineno;
    if (line.empty()) continue;
    if (line[0]=='#') continue;
    return line;
  }
  throw std::runtime_error(std::string("reached end of file '")+fname+std::string("' unexpectedly."));
}
//------------------------------------------------------------------------------------------------------
// Function to read AND parse the next content line
// * check that we really get N values of type T from the current line
template<typename T, int N>
typename std::vector<T>  ParseNextLine( std::istream& input, int& lineno, const std::string& fname ) {
  std::string line = ReadNextLine(input,lineno,fname);
  return parse_N_values_of_type_T<T,N>(line,lineno,fname);
}
//------------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
GateDoseActor::GateDoseActor(G4String name, G4int depth):
  GateVImageActor(name,depth) {
  GateDebugMessageInc("Actor",4,"GateDoseActor() -- begin\n");

  mCurrentEvent=-1;
  //Edep
  mIsEdepImageEnabled = false;
  mIsEdepSquaredImageEnabled = false;
  mIsEdepUncertaintyImageEnabled = false;
  //Dose  
  mIsDoseImageEnabled = true;
  mIsDoseSquaredImageEnabled = false;
  mIsDoseUncertaintyImageEnabled = false;
  mIsDoseNormalisationEnabled = false;
  mIsDoseEfficiencyEnabled = false;
  //DoseToWater
  mIsDoseToWaterImageEnabled = false;
  mIsDoseToWaterSquaredImageEnabled = false;
  mIsDoseToWaterUncertaintyImageEnabled = false;
  mIsDoseToWaterNormalisationEnabled = false;
  mDose2WaterWarningFlag = true;
  //DoseToOtherMaterial
  mIsDoseToOtherMaterialImageEnabled = false;
  mIsDoseToOtherMaterialSquaredImageEnabled = false;
  mIsDoseToOtherMaterialUncertaintyImageEnabled = false;
  mIsDoseToOtherMaterialNormalisationEnabled = false;
  mOtherMaterial = "G4Water";
  //Others    
  mIsNumberOfHitsImageEnabled = false;
  mIsLastHitEventImageEnabled = false;
  mDoseAlgorithmType = "VolumeWeighting";
  mImportMassImage = "";
  mExportMassImage = "";
  mVolumeFilter = "";
  mMaterialFilter = "";

  pMessenger = new GateDoseActorMessenger(this);
  GateDebugMessageDec("Actor",4,"GateDoseActor() -- end\n");
  emcalc = new G4EmCalculator;
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
/// Destructor
GateDoseActor::~GateDoseActor()  {
  delete pMessenger;
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
void GateDoseActor::EnableDoseNormalisationToMax(bool b) {
  mIsDoseNormalisationEnabled = b;
  mDoseImage.SetNormalizeToMax(b);
  mDoseImage.SetScaleFactor(1.0);
}
//-----------------------------------------------------------------------------
void GateDoseActor::EnableDoseNormalisationToIntegral(bool b) {
  mIsDoseNormalisationEnabled = b;
  mDoseImage.SetNormalizeToIntegral(b);
  mDoseImage.SetScaleFactor(1.0);
}
void GateDoseActor::EnableDoseToWaterNormalisationToMax(bool b) {
  mIsDoseToWaterNormalisationEnabled = b;
  mDoseToWaterImage.SetNormalizeToMax(b);
  mDoseToWaterImage.SetScaleFactor(1.0);
}
//-----------------------------------------------------------------------------
void GateDoseActor::EnableDoseToWaterNormalisationToIntegral(bool b) {
  mIsDoseToWaterNormalisationEnabled = b;
  mDoseToWaterImage.SetNormalizeToIntegral(b);
  mDoseToWaterImage.SetScaleFactor(1.0);
}//-----------------------------------------------------------------------------
void GateDoseActor::EnableDoseToOtherMaterialNormalisationToMax(bool b) {
  mIsDoseToOtherMaterialNormalisationEnabled = b;
  mDoseToOtherMaterialImage.SetNormalizeToMax(b);
  mDoseToOtherMaterialImage.SetScaleFactor(1.0);
}
//-----------------------------------------------------------------------------
void GateDoseActor::EnableDoseToOtherMaterialNormalisationToIntegral(bool b) {
  mIsDoseToOtherMaterialNormalisationEnabled = b;
  mDoseToOtherMaterialImage.SetNormalizeToIntegral(b);
  mDoseToOtherMaterialImage.SetScaleFactor(1.0);
}//-----------------------------------------------------------------------------
void GateDoseActor::SetEfficiencyFile(G4String b) {
  mDoseEfficiencyFile = b;
  mIsDoseEfficiencyEnabled = true; 
  }


//-----------------------------------------------------------------------------
/// Construct
void GateDoseActor::Construct() {
  GateDebugMessageInc("Actor", 4, "GateDoseActor -- Construct - begin\n");
  GateVImageActor::Construct();

  // Find G4_WATER. 
  G4NistManager::Instance()->FindOrBuildMaterial("G4_WATER");
  // Find OtherMaterial
  G4NistManager::Instance()->FindOrBuildMaterial(mOtherMaterial);
  // Record the stepHitType
  mUserStepHitType = mStepHitType;

  // Enable callbacks
  EnableBeginOfRunAction(true);
  EnableBeginOfEventAction(true);
  EnablePreUserTrackingAction(true);
  EnableUserSteppingAction(true);

  // Check if at least one image is enabled
  if (!mIsEdepImageEnabled &&
      !mIsDoseImageEnabled &&
      !mIsDoseToWaterImageEnabled &&
		!mIsDoseToOtherMaterialImageEnabled &&
      !mIsNumberOfHitsImageEnabled &&
      mExportMassImage=="")  {
    GateError("The DoseActor " << GetObjectName()
              << " does not have any image enabled ...\n Please select at least one ('enableEdep true' for example)");
  }

  // Output Filename
  mEdepFilename = G4String(removeExtension(mSaveFilename))+"-Edep."+G4String(getExtension(mSaveFilename));
  mDoseFilename = G4String(removeExtension(mSaveFilename))+"-Dose."+G4String(getExtension(mSaveFilename));
  mDoseToWaterFilename = G4String(removeExtension(mSaveFilename))+"-DoseToWater."+G4String(getExtension(mSaveFilename));
  mDoseToOtherMaterialFilename = G4String(removeExtension(mSaveFilename))+"-DoseToOtherMaterial_"+mOtherMaterial+"."+G4String(getExtension(mSaveFilename));
  mNbOfHitsFilename = G4String(removeExtension(mSaveFilename))+"-NbOfHits."+G4String(getExtension(mSaveFilename));

  // Set origin, transform, flag
  SetOriginTransformAndFlagToImage(mEdepImage);
  SetOriginTransformAndFlagToImage(mDoseImage);
  SetOriginTransformAndFlagToImage(mDoseToWaterImage);
  SetOriginTransformAndFlagToImage(mDoseToOtherMaterialImage);
  SetOriginTransformAndFlagToImage(mNumberOfHitsImage);
  SetOriginTransformAndFlagToImage(mLastHitEventImage);
  SetOriginTransformAndFlagToImage(mMassImage);

  // Resize and allocate images
  if (mIsEdepSquaredImageEnabled || mIsEdepUncertaintyImageEnabled ||
      mIsDoseSquaredImageEnabled || mIsDoseUncertaintyImageEnabled ||
      mIsDoseToWaterSquaredImageEnabled || mIsDoseToWaterUncertaintyImageEnabled ||
      mIsDoseToOtherMaterialSquaredImageEnabled || mIsDoseToOtherMaterialUncertaintyImageEnabled)
       {
    mLastHitEventImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
    mLastHitEventImage.Allocate();
    mIsLastHitEventImageEnabled = true;
  }
  //Edep
  if (mIsEdepImageEnabled) {
    //  mEdepImage.SetLastHitEventImage(&mLastHitEventImage);
    mEdepImage.EnableSquaredImage(mIsEdepSquaredImageEnabled);
    mEdepImage.EnableUncertaintyImage(mIsEdepUncertaintyImageEnabled);
    // Force the computation of squared image if uncertainty is enabled
    if (mIsEdepUncertaintyImageEnabled) mEdepImage.EnableSquaredImage(true);
    mEdepImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
    mEdepImage.Allocate();
    mEdepImage.SetFilename(mEdepFilename);
  }
  //Dose
  if (mIsDoseImageEnabled) {
    // mDoseImage.SetLastHitEventImage(&mLastHitEventImage);
    mDoseImage.EnableSquaredImage(mIsDoseSquaredImageEnabled);
    mDoseImage.EnableUncertaintyImage(mIsDoseUncertaintyImageEnabled);
    mDoseImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
    // Force the computation of squared image if uncertainty is enabled
    if (mIsDoseUncertaintyImageEnabled) mDoseImage.EnableSquaredImage(true);

    // DD(mDoseImage.GetVoxelVolume());
    //mDoseImage.SetScaleFactor(1e12/mDoseImage.GetVoxelVolume());
    mDoseImage.Allocate();
    mDoseImage.SetFilename(mDoseFilename);
  }
  //DoseToWater
  if (mIsDoseToWaterImageEnabled) {
    mDoseToWaterImage.EnableSquaredImage(mIsDoseToWaterSquaredImageEnabled);
    mDoseToWaterImage.EnableUncertaintyImage(mIsDoseToWaterUncertaintyImageEnabled);
    // Force the computation of squared image if uncertainty is enabled
    if (mIsDoseToWaterUncertaintyImageEnabled) mDoseToWaterImage.EnableSquaredImage(true);
    mDoseToWaterImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
    mDoseToWaterImage.Allocate();
    mDoseToWaterImage.SetFilename(mDoseToWaterFilename);
  }
//DoseToOtherMaterial
  if (mIsDoseToOtherMaterialImageEnabled) {
    mDoseToOtherMaterialImage.EnableSquaredImage(mIsDoseToOtherMaterialSquaredImageEnabled);
    mDoseToOtherMaterialImage.EnableUncertaintyImage(mIsDoseToOtherMaterialUncertaintyImageEnabled);
    // Force the computation of squared image if uncertainty is enabled
    if (mIsDoseToOtherMaterialUncertaintyImageEnabled) mDoseToOtherMaterialImage.EnableSquaredImage(true);
    mDoseToOtherMaterialImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
    mDoseToOtherMaterialImage.Allocate();
    mDoseToOtherMaterialImage.SetFilename(mDoseToOtherMaterialFilename);
  }
    if (mIsDoseEfficiencyEnabled){
 	std::ifstream inFile(mDoseEfficiencyFile);
    if (! inFile) {
      GateError("Cannot open dose efficiency file!");
    }
   std::vector<double> mDoseEfficiencyParameters;
   std::string line;
   int lineno = 0;
   int NbLines = ParseNextLine<int,1>(inFile,lineno,mDoseEfficiencyFile)[0];
//   std::cout<<NbLines<<std::endl;
   for (int k = 0; k < NbLines; k++) {
//    std::cout<<k<<std::endl;
    mDoseEfficiencyParameters = ParseNextLine<double,2>(inFile,lineno,mDoseEfficiencyFile);
  	 mDoseEnergy.push_back(mDoseEfficiencyParameters[0]);
    mDoseEfficiency.push_back(mDoseEfficiencyParameters[1]);
    GateMessage("Actor", 5, "[DoseActor] mDoseEfficiencyParameters: "<<mDoseEfficiencyParameters[0]<<"\t"<<mDoseEfficiencyParameters[1]<< Gateendl);
    if (k>0 && mDoseEnergy[k]<mDoseEnergy[k-1]){GateError("The energies of the Efficiency file must be ordered from lowest to highest - simulation abort!");}
	 }
    GateMessage("Actor", 0, "[DoseActor] : "<<mDoseEfficiencyFile<<" loaded successfully!"<< Gateendl);
  }
  //HIT
  if (mIsNumberOfHitsImageEnabled) {
    mNumberOfHitsImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
    mNumberOfHitsImage.Allocate();
  }
  
  if (mIsDoseImageEnabled &&
      (mExportMassImage != "" || mDoseAlgorithmType == "MassWeighting" ||
       mVolumeFilter != ""    || mMaterialFilter != "")) {
    mVoxelizedMass.SetMaterialFilter(mMaterialFilter);
    mVoxelizedMass.SetVolumeFilter(mVolumeFilter);
    mVoxelizedMass.SetExternalMassImage(mImportMassImage);
    mVoxelizedMass.Initialize(mVolumeName, &mDoseImage.GetValueImage());
    if (mExportMassImage != "") {
      mMassImage.SetResolutionAndHalfSize(mResolution, mHalfSize, mPosition);
      mMassImage.Allocate();
      mVoxelizedMass.UpdateImage(&mMassImage);
      mMassImage.Write(mExportMassImage);
    }
  }

  if (mExportMassImage!="" && mImportMassImage!="")
    GateWarning("Exported mass image will be the same as the imported one.");

  if (mDoseAlgorithmType != "MassWeighting") {
    mDoseAlgorithmType = "VolumeWeighting";

    if (mImportMassImage != "")
      GateWarning("importMassImage command is only compatible with MassWeighting algorithm. Ignored. ");
  }

  // Print information
  GateMessage("Actor", 1,
              "Dose DoseActor    = '" << GetObjectName() << "'\n" <<
              "\tDose image        = " << mIsDoseImageEnabled << Gateendl <<
              "\tDose squared      = " << mIsDoseSquaredImageEnabled << Gateendl <<
              "\tDose uncertainty  = " << mIsDoseUncertaintyImageEnabled << Gateendl <<
              "\tDose to water image        = " << mIsDoseToWaterImageEnabled << Gateendl <<
              "\tDose to water squared      = " << mIsDoseToWaterSquaredImageEnabled << Gateendl <<
              "\tDose to water uncertainty  = " << mIsDoseToWaterUncertaintyImageEnabled << Gateendl <<
              "\tEdep image        = " << mIsEdepImageEnabled << Gateendl <<
              "\tEdep squared      = " << mIsEdepSquaredImageEnabled << Gateendl <<
              "\tEdep uncertainty  = " << mIsEdepUncertaintyImageEnabled << Gateendl <<
              "\tNumber of hit     = " << mIsNumberOfHitsImageEnabled << Gateendl <<
              "\t     (last hit)   = " << mIsLastHitEventImageEnabled << Gateendl <<
              "\tDose algorithm    = " << mDoseAlgorithmType << Gateendl <<
              "\tMass image (import) = " << mImportMassImage << Gateendl <<
              "\tMass image (export) = " << mExportMassImage << Gateendl <<
              "\tEdepFilename      = " << mEdepFilename << Gateendl <<
              "\tDoseFilename      = " << mDoseFilename << Gateendl <<
              "\tNb Hits filename  = " << mNbOfHitsFilename << Gateendl);

  ResetData();
  GateMessageDec("Actor", 4, "GateDoseActor -- Construct - end\n");
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
/// Save data
void GateDoseActor::SaveData() {
  GateVActor::SaveData(); // (not needed because done into GateImageWithStatistic)
  //Edep
  if (mIsEdepImageEnabled) mEdepImage.SaveData(mCurrentEvent+1);
  //Dose
  if (mIsDoseImageEnabled) {
    if (mIsDoseNormalisationEnabled)
      mDoseImage.SaveData(mCurrentEvent+1, true);
    else
      mDoseImage.SaveData(mCurrentEvent+1, false);
  }
  //DoseToWater
  if (mIsDoseToWaterImageEnabled) {
    if (mIsDoseToWaterNormalisationEnabled)
      mDoseToWaterImage.SaveData(mCurrentEvent+1, true);
    else
      mDoseToWaterImage.SaveData(mCurrentEvent+1, false);
  }
  //DoseToOtherMaterial
  if (mIsDoseToOtherMaterialImageEnabled) {
    if (mIsDoseToOtherMaterialNormalisationEnabled)
      mDoseToOtherMaterialImage.SaveData(mCurrentEvent+1, true);
    else
      mDoseToOtherMaterialImage.SaveData(mCurrentEvent+1, false);
  }
  
  if (mIsLastHitEventImageEnabled) {
    mLastHitEventImage.Fill(-1); // reset
  }

  if (mIsNumberOfHitsImageEnabled) {
    mNumberOfHitsImage.Write(mNbOfHitsFilename);
  }
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
void GateDoseActor::ResetData() {
  if (mIsLastHitEventImageEnabled) mLastHitEventImage.Fill(-1);
  if (mIsEdepImageEnabled) mEdepImage.Reset();
  if (mIsDoseImageEnabled) mDoseImage.Reset();
  if (mIsDoseToWaterImageEnabled) mDoseToWaterImage.Reset();
  if (mIsDoseToOtherMaterialImageEnabled) mDoseToOtherMaterialImage.Reset();
  if (mIsNumberOfHitsImageEnabled) mNumberOfHitsImage.Fill(0);
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
void GateDoseActor::BeginOfRunAction(const G4Run * r) {
  GateVActor::BeginOfRunAction(r);
  GateDebugMessage("Actor", 3, "GateDoseActor -- Begin of Run\n");
  mDose2WaterWarningFlag = true;
  // ResetData(); // Do no reset here !! (when multiple run);
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Callback at each event
void GateDoseActor::BeginOfEventAction(const G4Event * e) {
  GateVActor::BeginOfEventAction(e);
  mCurrentEvent++;
  GateDebugMessage("Actor", 3, "GateDoseActor -- Begin of Event: "<< mCurrentEvent << Gateendl);
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
void GateDoseActor::UserPreTrackActionInVoxel(const int /*index*/, const G4Track* track)
{
  if(track->GetDefinition() == G4Gamma::Gamma()) { mStepHitType = PostStepHitType; }
  else { mStepHitType = mUserStepHitType; }
}
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
void GateDoseActor::UserSteppingActionInVoxel(const int index, const G4Step* step) {
  GateDebugMessageInc("Actor", 4, "GateDoseActor -- UserSteppingActionInVoxel - begin\n");
  GateDebugMessageInc("Actor", 4, "enedepo = " << step->GetTotalEnergyDeposit() << Gateendl);
  GateDebugMessageInc("Actor", 4, "weight = " <<  step->GetTrack()->GetWeight() << Gateendl);
  const double weight = step->GetTrack()->GetWeight();
  const double edep = step->GetTotalEnergyDeposit()*weight;//*step->GetTrack()->GetWeight();

  // if no energy is deposited or energy is deposited outside image => do nothing
  if (edep == 0) {
    GateDebugMessage("Actor", 5, "edep == 0 : do nothing\n");
    GateDebugMessageDec("Actor", 4, "GateDoseActor -- UserSteppingActionInVoxel -- end\n");
    return;
  }
  if (index < 0) {
    GateDebugMessage("Actor", 5, "index < 0 : do nothing\n");
    GateDebugMessageDec("Actor", 4, "GateDoseActor -- UserSteppingActionInVoxel -- end\n");
    return;
  }

  if (mVolumeFilter != "" && mVolumeFilter+"_phys" != step->GetPreStepPoint()->GetPhysicalVolume()->GetName())
    return;

  if (mMaterialFilter != "" && mMaterialFilter != step->GetPreStepPoint()->GetMaterial()->GetName())
    return;

  // compute sameEvent
  // sameEvent is false the first time some energy is deposited for each primary particle
  bool sameEvent=true;
  if (mIsLastHitEventImageEnabled) {
    GateDebugMessage("Actor", 2,  "GateDoseActor -- UserSteppingActionInVoxel: Last event in index = " << mLastHitEventImage.GetValue(index) << Gateendl);
    if (mCurrentEvent != mLastHitEventImage.GetValue(index)) {
      sameEvent = false;
      mLastHitEventImage.SetValue(index, mCurrentEvent);
    }
  }

  //---------------------------------------------------------------------------------
  // Volume weighting
  double density = step->GetPreStepPoint()->GetMaterial()->GetDensity();
  //---------------------------------------------------------------------------------

  //---------------------------------------------------------------------------------
  // Mass weighting OR filter
  if (mDoseAlgorithmType == "MassWeighting" || mMaterialFilter != "" || mVolumeFilter != "")
    density = mVoxelizedMass.GetDoselMass(index)/mDoseImage.GetVoxelVolume();
  //---------------------------------------------------------------------------------

  if (mMaterialFilter != "") {
    GateDebugMessage("Actor", 3,  "GateDoseActor -- UserSteppingActionInVoxel: material filter debug = " << Gateendl
                     << " material name        = " << step->GetPreStepPoint()->GetMaterial()->GetName() << Gateendl
                     << " density              = " << G4BestUnit(mVoxelizedMass.GetPartialMassWithMatName(index)/mVoxelizedMass.GetPartialVolumeWithMatName(index), "Volumic Mass") << Gateendl
                     << " dosel cubic volume   = " << G4BestUnit(mDoseImage.GetVoxelVolume(), "Volume") << Gateendl
                     << " partial cubic volume = " << G4BestUnit(mVoxelizedMass.GetPartialVolumeWithMatName(index), "Volume") << Gateendl);
  }

  if (mVolumeFilter != "") {
    GateDebugMessage("Actor", 3,  "GateDoseActor -- UserSteppingActionInVoxel: volume filter debug = " << Gateendl
                     << " volume name          = " << step->GetPreStepPoint()->GetPhysicalVolume()->GetName() << Gateendl
                     << " Dose scored inside volume filtered volume !" << Gateendl);
  }

  //Edep
  if (mIsEdepImageEnabled) {
    GateDebugMessage("Actor", 2, "GateDoseActor -- UserSteppingActionInVoxel:\tedep = " << G4BestUnit(edep, "Energy") << Gateendl);
  }
  
  //Dose
  double dose=0.;
  if (mIsDoseImageEnabled) {
    // ------------------------------------
    // Convert deposited energy into Gray
    dose = edep/density/mDoseImage.GetVoxelVolume()/gray;
    // ------------------------------------
    
    if(mIsDoseEfficiencyEnabled){
		 double energy1 = step->GetPreStepPoint()->GetKineticEnergy();
		 double energy2 = step->GetPostStepPoint()->GetKineticEnergy();
		 double energy=(energy1+energy2)/2;
		 double efficiency=1;
		 for (unsigned int k=0; k<mDoseEnergy.size(); k++){
		 	if(mDoseEnergy[k]>energy){
		 		efficiency=mDoseEfficiency[k-1]+(mDoseEfficiency[k]-mDoseEfficiency[k-1])/(mDoseEnergy[k]-mDoseEnergy[k-1])*(energy-mDoseEnergy[k-1]);
		 		k=mDoseEnergy.size();
		 	}
		 	else if(k==mDoseEnergy.size()-1){
			GateMessage("Actor", 0, "WARNING particle energy larger than energies available in the file: "<<mDoseEfficiencyFile<<" Efficiency = 1 instead"<<Gateendl);			
		 	}
		 }
		 if(mTestFlag){
			const G4ParticleDefinition* partname = step->GetTrack()->GetDefinition();//->GetParticleName();
			const G4Material* material = step->GetPreStepPoint()->GetMaterial();//->GetName();
			G4double dedx = emcalc->ComputeElectronicDEDX(energy, partname, material);
			
		 	G4cout<<"Particle : "<<partname->GetParticleName()<<"\t energy : "<<energy<<"\t material : "<<material->GetName()<<"\t dedx : "<<dedx<<"\t efficiency : "<<efficiency<<"\t dose : "<<dose; 
		 }
		 dose*=efficiency;
		 if(mTestFlag){G4cout<<"\t effective dose : "<<dose<<G4endl;} 

    }

    GateDebugMessage("Actor", 2,  "GateDoseActor -- UserSteppingActionInVoxel:\tdose = "
                     << G4BestUnit(dose, "Dose")
                     << " rho = "
                     << G4BestUnit(density, "Volumic Mass")<< Gateendl );
  }

  //DoseToWater
  double doseToWater = 0;
  if (mIsDoseToWaterImageEnabled)
    {
     double cut = DBL_MAX;
	//get the energy
	  double energy1 = step->GetPreStepPoint()->GetKineticEnergy();
	  double energy2 = step->GetPostStepPoint()->GetKineticEnergy();
	  double energy=(energy1+energy2)/2;
	//Get current particle
     const G4ParticleDefinition * p = step->GetTrack()->GetParticleDefinition();
	// dedx
     double DEDX=0, DEDX_Water=0;
	//current material     
	  G4Material * material = step->GetPreStepPoint()->GetMaterial();
     double density = material->GetDensity();
	//other material
     static G4Material * water = G4NistManager::Instance()->FindOrBuildMaterial("G4_WATER");

	//Accounting for particles with dedx=0; i.e. gamma and neutrons
	//For gamma we consider the dedx of electrons instead - testing with 1.3 MeV photon beam or 150 MeV protons or 1500 MeV carbon ion beam showed that the error induced is 0
	//		when comparing dose and dosetowater in the material G4_WATER
	//For neutrons the dose is neglected - testing with 1.3 MeV photon beam or 150 MeV protons or 1500 MeV carbon ion beam showed that the error induced is < 0.01%
	//		when comparing dose and dosetowater in the material G4_WATER (we are systematically missing a little bit of dose of course with this solution)
		if (p == G4Gamma::Gamma()) {
      	  p = G4Electron::Electron();
        }
        DEDX = emcalc->ComputeTotalDEDX(energy, p, material, cut);
        DEDX_Water = emcalc->ComputeTotalDEDX(energy, p, water, cut);
	//In current implementation, dose deposited directly by neutrons is neglected - the below lines prevent "inf or NaN" 	    
      if (DEDX==0 || DEDX_Water==0){
      	doseToWater=0;
      	}
        else{
      	  doseToWater = dose*(DEDX_Water/1.0)/(DEDX/(density*e_SI));
        }
//FIXME
//remove this flag
/*
        if (mDose2WaterWarningFlag) {
          GateMessage("Actor", 0, "WARNING: DoseToWater with a particle which is not proton/electron/positron/gamma/deuteron: results could be wrong." << G4endl);
          mDose2WaterWarningFlag = false;
        }
*/
      GateDebugMessage("Actor", 2,  "GateDoseActor -- UserSteppingActionInVoxel:\tdose to water = "
                       << G4BestUnit(doseToWater, "Dose to water")
                       << " rho = "
                       << G4BestUnit(density, "Volumic Mass")<< Gateendl );
    }

  //DoseToOtherMaterial
  double DoseToOtherMaterial = 0;
  if (mIsDoseToOtherMaterialImageEnabled){
      double cut = DBL_MAX;
	 	G4String material;
		const G4MaterialTable* matTbl = G4Material::GetMaterialTable();
		bool IsMaterialInMDB = false;
		double Density_OtherMaterial = 1;

  // check if the material has already been created in the simulation		
  	 for(size_t k=0; k < G4Material::GetNumberOfMaterials(); k++){
        material = (*matTbl)[k]->GetName();
        if(material==mOtherMaterial){
      	 IsMaterialInMDB=true;
      	 Density_OtherMaterial = (*matTbl)[k]->GetDensity();
      	 k=G4Material::GetNumberOfMaterials();
      	} 
      }
   // create the material if not already created in the simulation
	 if(!IsMaterialInMDB){
			//FIXME
			//CREATE THE MISSING MATERIAL (look into the Gate db)
			GateError("Material not defined - abort simulation");
		}

	//get the energy
	  double energy1 = step->GetPreStepPoint()->GetKineticEnergy();
	  double energy2 = step->GetPostStepPoint()->GetKineticEnergy();
	  double energy=(energy1+energy2)/2;
	//Get current particle
     const G4ParticleDefinition * p = step->GetTrack()->GetParticleDefinition();
	// dedx
     double DEDX=0, DEDX_OtherMaterial=0;
	//current material     
     G4Material * current_material = step->GetPreStepPoint()->GetMaterial();
     double current_density = current_material->GetDensity();
	//other material
    static G4Material* OtherMaterial = G4Material::GetMaterial(mOtherMaterial,true);     

	//Accounting for particles with dedx=0; i.e. gamma and neutrons
	//For gamma we consider the dedx of electrons instead - testing with 1.3 MeV photon beam or 150 MeV protons or 1500 MeV carbon ion beam showed that the error induced is 0
	//		when comparing dose and dosetowater in the material G4_WATER
	//For neutrons the dose is neglected - testing with 1.3 MeV photon beam or 150 MeV protons or 1500 MeV carbon ion beam showed that the error induced is < 0.01%
	//		we are systematically missing a little bit of dose of course with this solution
		if (p == G4Gamma::Gamma()) {
      	  p = G4Electron::Electron();
        }
      DEDX = emcalc->ComputeTotalDEDX(energy, p, current_material, cut);
      DEDX_OtherMaterial = emcalc->ComputeTotalDEDX(energy, p, OtherMaterial, cut);
	//In current implementation, dose deposited directly by neutrons is neglected - the below lines prevent "inf or NaN" 	    
      if (DEDX==0 || DEDX_OtherMaterial==0){
      	DoseToOtherMaterial=0;
      	}
        else{
      	  DoseToOtherMaterial = dose*(DEDX_OtherMaterial/(Density_OtherMaterial*e_SI))/(DEDX/(current_density*e_SI));
        }
      	  
/*
// DISPLAY parameters of particles having DEDX=0
// Mainly gamma and neutron
if(DEDX==0){
      const G4ParticleDefinition* partname = step->GetTrack()->GetDefinition();//->GetParticleName();
	 	G4cout<<"Particle : "<<partname->GetParticleName()<<"\t energy : "<<energy<<"\t current material : "<<current_material->GetName()<<"\t dedx : "<<DEDX<<"\t density : "<<current_density*e_SI<<"\t dose : "<<dose<<G4endl;
	 	G4cout<<"Particle : "<<partname->GetParticleName()<<"\t energy : "<<energy<<"\t other material : "<<mOtherMaterial<<"\t dedx other : "<<DEDX_OtherMaterial<<"\t density other : "<<Density_OtherMaterial*e_SI<<"\t dose to other: "<<DoseToOtherMaterial<<G4endl; 

// DISPLAY the process involved
 G4ProcessVector* plist = partname->GetProcessManager()->GetProcessList();
 for (G4int j = 0; j < plist->size(); j++)
    {
    	G4cout<<"Process type : "<<(*plist)[j]->GetProcessType()<<"\t process name : "<<(*plist)[j]->GetProcessName()<<G4endl;
    }
}
*/

      GateDebugMessage("Actor", 2,  "GateDoseActor -- UserSteppingActionInVoxel:\tdose to OtherMaterial = "
                       << G4BestUnit(DoseToOtherMaterial, "Dose to OtherMaterial")
                       << " rho = "
                       << G4BestUnit(density, "Volumic Mass")<< Gateendl );
    }

  //Edep
  if (mIsEdepImageEnabled)
    {
      if (mIsEdepUncertaintyImageEnabled || mIsEdepSquaredImageEnabled)
        {
          if (sameEvent) mEdepImage.AddTempValue(index, edep);
          else mEdepImage.AddValueAndUpdate(index, edep);
        }
      else
        {
          mEdepImage.AddValue(index, edep);

        }
    }

  //Dose    
  if (mIsDoseImageEnabled)
    {
      if (mIsDoseUncertaintyImageEnabled || mIsDoseSquaredImageEnabled)
        {
          if (sameEvent) mDoseImage.AddTempValue(index, dose);
          else mDoseImage.AddValueAndUpdate(index, dose);
        }
      else mDoseImage.AddValue(index, dose);
    }

  //DoseToWater
  if (mIsDoseToWaterImageEnabled)
    {
      if (mIsDoseToWaterUncertaintyImageEnabled || mIsDoseToWaterSquaredImageEnabled)
        {
          if (sameEvent) mDoseToWaterImage.AddTempValue(index, doseToWater);
          else mDoseToWaterImage.AddValueAndUpdate(index, doseToWater);
        }
      else mDoseToWaterImage.AddValue(index, doseToWater);
    }

  //DoseToOtherMaterial
  if (mIsDoseToOtherMaterialImageEnabled)
    {
      if (mIsDoseToOtherMaterialUncertaintyImageEnabled || mIsDoseToOtherMaterialSquaredImageEnabled)
        {
          if (sameEvent) mDoseToOtherMaterialImage.AddTempValue(index, DoseToOtherMaterial);
          else mDoseToOtherMaterialImage.AddValueAndUpdate(index, DoseToOtherMaterial);
        }
      else mDoseToOtherMaterialImage.AddValue(index, DoseToOtherMaterial);
    }
    
  if (mIsNumberOfHitsImageEnabled) mNumberOfHitsImage.AddValue(index, weight);

  GateDebugMessageDec("Actor", 4, "GateDoseActor -- UserSteppingActionInVoxel -- end\n");
}
//-----------------------------------------------------------------------------
