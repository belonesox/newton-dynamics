/* Copyright (c) <2003-2022> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#ifndef __D_VEHICLE_COMMON_UTIL__
#define __D_VEHICLE_COMMON_UTIL__

#include "ndSandboxStdafx.h"
#include "ndDemoEntity.h"
#include "ndContactCallback.h"

#define AUTOMATION_TRANSMISSION_FRAME_DELAY 120

class ndVehicleDectriptor
{
	public:
	class ndTorqueTap
	{
		public:
		ndTorqueTap() {}
		ndTorqueTap(ndFloat32 rpm, ndFloat32 torqueInPoundFeet)
			:m_radPerSeconds(rpm * 0.105f)
			,m_torqueInNewtonMeters(torqueInPoundFeet * 1.36f)
		{
		}
		ndFloat32 m_radPerSeconds;
		ndFloat32 m_torqueInNewtonMeters;
	};

	class ndEngineTorqueCurve
	{
		public:
		ndEngineTorqueCurve();
	
		void Init(ndFloat32 idleTorquePoundFoot, ndFloat32 idleRmp,
			ndFloat32 horsePower, ndFloat32 rpm0, ndFloat32 rpm1,
			ndFloat32 horsePowerAtRedLine, ndFloat32 redLineRpm);

		ndFloat32 GetIdleRadPerSec() const;
		ndFloat32 GetRedLineRadPerSec() const;
		ndFloat32 GetLowGearShiftRadPerSec() const;
		ndFloat32 GetHighGearShiftRadPerSec() const;
		ndFloat32 GetTorque(ndFloat32 omegaInRadPerSeconds) const;
	
		private:
		ndTorqueTap m_torqueCurve[5];
		//ndFloat32 m_fuelInjectionRate;
	};

	class ndGearBox
	{
		public:
		ndInt32 m_gearsCount;
		union
		{
			struct
			{
				ndFloat32 m_forwardRatios[5];
				ndFloat32 m_reverseRatio;
				ndFloat32 m_neutral;
			};
			ndFloat32 m_ratios[8];
		};

		ndFloat32 m_idleClutchTorque;
		ndFloat32 m_lockedClutchTorque;
		ndFloat32 m_crownGearRatio;
		ndFloat32 m_torqueConverter;
		bool m_manual;
	};

	class ndTireDefinition: public ndMultiBodyVehicleTireJointInfo
	{
		public:
		ndFloat32 m_mass;
		ndFloat32 m_verticalOffset;
	};

	enum ndDifferentialType
	{
		m_rearWheelDrive,
		m_frontWheelDrive,
		m_fourWheeldrive,
		m_eightWheeldrive,
	};

	enum ndTorsionBarType
	{
		m_noWheelAxle,
		m_rearWheelAxle,
		m_frontWheelAxle,
		m_fourWheelAxle,
	};

	ndVehicleDectriptor() {}
	ndVehicleDectriptor(const char* const fileName);

	ndVector m_comDisplacement;
	char m_name[32];

	ndFloat32 m_chassisMass;
	ndFloat32 m_chassisAngularDrag;
	ndEngineTorqueCurve m_engine;
	ndGearBox m_transmission;
	ndTireDefinition m_frontTire;
	ndTireDefinition m_rearTire;
	ndFloat32 m_motorMass;
	ndFloat32 m_motorRadius;

	ndFloat32 m_differentialMass;
	ndFloat32 m_differentialRadius;
	ndFloat32 m_slipDifferentialRmpLock;
	ndDifferentialType m_differentialType;

	ndFloat32 m_torsionBarSpringK;
	ndFloat32 m_torsionBarDamperC;
	ndFloat32 m_torsionBarRegularizer;
	ndTorsionBarType m_torsionBarType;

	bool m_useHardSolverMode;
};

class ndVehicleMaterial : public ndApplicationMaterial
{
	public:
	ndVehicleMaterial();
	ndVehicleMaterial(const ndVehicleMaterial& src);
	ndVehicleMaterial* Clone() const;
	void OnContactCallback(const ndContact* const joint, ndFloat32) const;
	bool OnAabbOverlap(const ndContact* const joint, ndFloat32 timestep, const ndShapeInstance& instanceShape0, const ndShapeInstance& instanceShape1) const;
};

class ndBasicVehicle : public ndMultiBodyVehicle
{
	public:
	D_CLASS_REFLECTION(ndBasicVehicle);

	ndBasicVehicle(const ndVehicleDectriptor& desc);
	virtual ~ndBasicVehicle();

	bool IsPlayer() const;
	virtual void SetAsPlayer(ndDemoEntityManager* const scene, bool mode = true);

	protected:
	void Update(ndWorld* const world, ndFloat32 timestep);
	void PostUpdate(ndWorld* const world, ndFloat32 timestep);

	void ApplyInputs(ndWorld* const world, ndFloat32 timestep);

	void SetChassis(ndBodyDynamic* const chassis);
	void CalculateTireDimensions(const char* const tireName, ndFloat32& width, ndFloat32& radius, ndDemoEntity* const vehEntity) const;
	ndBodyDynamic* CreateTireBody(ndDemoEntityManager* const scene, ndBodyDynamic* const parentBody, ndVehicleDectriptor::ndTireDefinition& definition, const char* const tireName) const;

	ndVehicleDectriptor m_configuration;
	ndFloat32 m_steerAngle;

	ndDemoEntityManager::ndKeyTrigger m_parking;
	ndDemoEntityManager::ndKeyTrigger m_ignition;
	ndDemoEntityManager::ndKeyTrigger m_neutralGear;
	ndDemoEntityManager::ndKeyTrigger m_reverseGear;
	ndDemoEntityManager::ndKeyTrigger m_forwardGearUp;
	ndDemoEntityManager::ndKeyTrigger m_forwardGearDown;
	ndDemoEntityManager::ndKeyTrigger m_manualTransmission;

	ndInt32 m_currentGear;
	ndInt32 m_autoGearShiftTimer;
	bool m_isPlayer;
	bool m_isParked;
	bool m_startEngine;
	bool m_startEngineMemory;
	bool m_isManualTransmission;
};

class ndVehicleSelector : public ndModel
{
	public:
	D_CLASS_REFLECTION(ndVehicleSelector);
	ndVehicleSelector();
	ndVehicleSelector(const ndLoadSaveBase::ndLoadDescriptor& desc);

	void Update(ndWorld* const, ndFloat32){}
	void PostUpdate(ndWorld* const world, ndFloat32);
	void Save(const ndLoadSaveBase::ndSaveDescriptor& desc) const;

	ndDemoEntityManager::ndKeyTrigger m_changeVehicle;
};

#endif