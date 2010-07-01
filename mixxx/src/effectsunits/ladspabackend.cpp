#include "ladspabackend.h"

LADSPABackend::LADSPABackend() {
	qDebug() << "FXUNITS: LADSPABackend: " << this;

	PluginIDSequence = 0;
	m_monoBufferSize = 0;
	m_LADSPALoader = new LADSPALoader();

//	LADSPAPlugin * flanger = loader->getByLabel("Plate2x2");
//	LADSPAInstance * flangerInstance = flanger->instantiate(0);
//
//	m_test = flangerInstance;
//
//
//    zero = new LADSPAControl();
//    um = new LADSPAControl();
//    dois = new LADSPAControl();
//    tres = new LADSPAControl();
//
//    zero->setValue(0.799);
//    um->setValue(0.745);
//    dois->setValue(0.8);
//    tres->setValue(0.9);
//
//    m_test->connect(2, zero->getBuffer());
//    m_test->connect(3, um->getBuffer());
//    m_test->connect(4, dois->getBuffer());
//    m_test->connect(5, tres->getBuffer());

}

LADSPABackend::~LADSPABackend() {
	// TODO Auto-generated destructor stub
}

void LADSPABackend::loadPlugins(){

	EffectsUnitsPlugin * plugin;
	LADSPAPlugin * ladspaplugin;
	EffectsUnitsPort * port;

	int i = 0;
	ladspaplugin = m_LADSPALoader->getByIndex(i);

	while (ladspaplugin != NULL){

		plugin = new EffectsUnitsPlugin(this, new QString(ladspaplugin->getLabel()), PluginIDSequence++);

		int j = 0;
		int port_count = ladspaplugin->getDescriptor()->PortCount;
		for (j = 0; j < port_count; j++){
			port = new EffectsUnitsPort();

			if (LADSPA_IS_PORT_AUDIO(ladspaplugin->getDescriptor()->PortDescriptors[j])){
				port->isAudio = true;
			} else {
				port->isAudio = false;
				port->isBound = false;
				port->Max = ladspaplugin->getDescriptor()->PortRangeHints[j].UpperBound;
				port->Min = ladspaplugin->getDescriptor()->PortRangeHints[j].LowerBound;
				port->Def = port->Max;
				port->Name = new QString(ladspaplugin->getDescriptor()->PortNames[j]);
			}

			plugin->addPort(port);
		}

		m_BackendPlugins.push_back(plugin);
		m_LADSPAPlugin.push_back(ladspaplugin);
		m_LADSPAInstance.push_back(NULL);
		m_PluginLADSPAControl.push_back(NULL);

		i++;

		ladspaplugin = m_LADSPALoader->getByIndex(i);
	}

}

void LADSPABackend::process(const CSAMPLE *pIn, const CSAMPLE *pOut, const int iBufferSize, int PluginID){
	/* Invalid PluginID, NOP */
	if (PluginID >= PluginIDSequence){ return; }
	qDebug() << "FXUNITS: LADSPABackend: Processing: " << m_BackendPlugins.at(PluginID)->getName();


	/* Initial Set-up */
	if (m_monoBufferSize != iBufferSize/2){
		m_monoBufferSize = iBufferSize/2;
		LADSPAControl::setBufferSize(m_monoBufferSize);

//		if (m_pBufferLeft[0] != NULL){
//			delete [] m_pBufferLeft[0];
//			delete [] m_pBufferLeft[1];
//			delete [] m_pBufferRight[0];
//			delete [] m_pBufferRight[1];
//		}
		m_pBufferLeft[0] = new CSAMPLE[m_monoBufferSize];
		m_pBufferLeft[1] = new CSAMPLE[m_monoBufferSize];
		m_pBufferRight[0] = new CSAMPLE[m_monoBufferSize];
		m_pBufferRight[1] = new CSAMPLE[m_monoBufferSize];
	}

    for (int i = 0; i < m_monoBufferSize; i++)
    {
        m_pBufferLeft[0][i] = pIn[2 * i];
        m_pBufferRight[0][i] = pIn[2 * i + 1];
    }

    /* Process Controls: */
    m_beingUpdated = m_PluginLADSPAControl.at(PluginID);
    m_beingRead = m_BackendPlugins.at(PluginID)->getPorts();

    int size = m_beingRead->size();
    for (int i = 0; i < size; i++){
    	if (!m_beingRead->at(i)->isAudio){
    		if (m_beingRead->at(i)->isBound){
    			// TODO - get value from control object
    		} else {
    			m_beingUpdated->at(i)->setValue(m_beingRead->at(i)->Def);
    		}
    	}
    }

    /* Process Audio Signals: */
    m_beingProcessed = m_LADSPAInstance.at(PluginID);
	if (m_beingProcessed->isInplaceBroken())
	{
		m_beingProcessed->process(m_pBufferLeft[0], m_pBufferRight[0], m_pBufferLeft[1], m_pBufferRight[1], m_monoBufferSize);
		qDebug() << "FXUNITS: LADSPABackend::process: INP: " << *m_pBufferLeft[0] << "OUT: " << *m_pBufferLeft[1] << "BUF IPB: " << iBufferSize;

		for (int i = 0; i < m_monoBufferSize; i++)
		{
			m_pBufferLeft [0][i] = m_pBufferLeft [0][i] * 0.2 + m_pBufferLeft [1][i] * 0.8; // TODO - Make Dry/Wet
			m_pBufferRight[0][i] = m_pBufferRight[0][i] * 0.2 + m_pBufferRight[1][i] * 0.8;
		}
	} else {
		qDebug() << "FXUNITS: LADSPABackend::process: IN: " << *m_pBufferLeft[0];
		m_beingProcessed->process(m_pBufferLeft[0], m_pBufferRight[0], m_pBufferLeft[0], m_pBufferRight[0], m_monoBufferSize);
		qDebug() << "FXUNITS: LADSPABackend::process: OUT: " << *m_pBufferLeft[0];
	}


	/* Creating Final Output */
	CSAMPLE * pOutput = (CSAMPLE *)pOut;
	for (int j = 0; j < m_monoBufferSize; j++)
	{
		pOutput[2 * j]     = m_pBufferLeft [0][j];
		pOutput[2 * j + 1] = m_pBufferRight[0][j];
	}
}

/* LADSPABackend::activatePlugin
 * Given a correct PluginID of an unactivated plugin, this is what we're doing to do:
 * Turn LADSPAPlugin into LADSPAInstance (which has process())
 * Connect the ports of the instance to LADSPAControls, so we can tweak values
 */
void LADSPABackend::activatePlugin(int PluginID){
	if (m_LADSPAInstance.at(PluginID) == NULL && PluginID < PluginIDSequence){

		/* Instantiates the plugin, so we can process it */
		EffectsUnitsPlugin * fxplugin = m_BackendPlugins.at(PluginID);
		LADSPAInstance * instance = m_LADSPAPlugin.at(PluginID)->instantiate(0);
		m_LADSPAInstance.replace(PluginID, instance);

		/* Handle plugins ports */
		QList<EffectsUnitsPort *> * ports = fxplugin->getPorts();
		QList<LADSPAControl *> * controls = new QList<LADSPAControl *>();
		LADSPAControl * current;

		int size = ports->size();
		for (int i = 0; i < size; i++){
			/* If its an audio port, it doesnt need a control */
			if (ports->at(i)->isAudio){
				controls->push_back(NULL);

			/* Creates a new LADSPAControl, connects its buffer to the plugin, assign default value */
			} else {
				current = new LADSPAControl();
				current->setValue(ports->at(i)->Def);
				instance->connect(i, current->getBuffer());
				controls->push_back(current);
			}
		}
		/* Adds the list of controls to be processed */
		m_PluginLADSPAControl.replace(PluginID, controls);

		qDebug() << "FXUNITS: LADSPABackend: Activating: " << fxplugin->getName();

	}
}

void LADSPABackend::deactivatePlugin(int PluginID){
	//TODO - Turn plugin into instance

}

