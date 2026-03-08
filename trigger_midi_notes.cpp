/*
 * Trigger MIDI Notes - distingNT Plugin
 * GUID: ThTm (Thorinside Trigger Midi)
 *
 * Converts CV triggers/gates on up to 8 channels into MIDI note on/off messages.
 * Each channel has configurable input, MIDI channel, MIDI destination, and note number.
 * The number of channels is set via a specification (1-8).
 */

#include <distingnt/api.h>
#include <new>
#include <string.h>

static const int kMaxChannels = 8;
// Schmitt trigger thresholds (Eurorack standard: ~1V high, ~0.1V low)
static const float kThresholdHigh = 1.0f;
static const float kThresholdLow = 0.1f;

// Per-channel parameter offsets
enum {
	kChParamInput = 0,
	kChParamMidiChannel,
	kChParamMidiDest,
	kChParamMidiNote,
	kNumChParams,
};

static const char* destStrings[] = {
	"Breakout",
	"USB",
	"Select Bus",
	"Internal",
	NULL,
};

// Build the full parameter array for max channels
// Total params = kMaxChannels * kNumChParams
static const _NT_parameter channelParamTemplate[] = {
	{ .name = "Input", .min = 0, .max = 28, .def = 0, .unit = kNT_unitAudioInput, .scaling = 0, .enumStrings = NULL },
	{ .name = "MIDI Ch", .min = 1, .max = 16, .def = 1, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL },
	{ .name = "MIDI Dest", .min = 0, .max = 3, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = destStrings },
	{ .name = "Note", .min = 0, .max = 127, .def = 60, .unit = kNT_unitMIDINote, .scaling = 0, .enumStrings = NULL },
};

// We need a flat array of all parameters for up to 8 channels.
// Since these are repeated per-channel, we build them in a static array.
static _NT_parameter parameters[kMaxChannels * kNumChParams];
static bool parametersInitialized = false;

static void initParameters() {
	if (parametersInitialized) return;
	for (int ch = 0; ch < kMaxChannels; ++ch) {
		for (int p = 0; p < kNumChParams; ++p) {
			parameters[ch * kNumChParams + p] = channelParamTemplate[p];
			// Set default input bus: channel 0 -> bus 1, channel 1 -> bus 2, etc.
			if (p == kChParamInput) {
				parameters[ch * kNumChParams + p].def = ch + 1;
			}
		}
	}
	parametersInitialized = true;
}

// Parameter pages: one page per channel, dynamic based on spec
static uint8_t pageIndices[kMaxChannels][kNumChParams];
static _NT_parameterPage pageArray[kMaxChannels];
static const char* pageNames[] = { "Ch 1", "Ch 2", "Ch 3", "Ch 4", "Ch 5", "Ch 6", "Ch 7", "Ch 8" };

static void initPages(int numChannels) {
	for (int ch = 0; ch < numChannels; ++ch) {
		for (int p = 0; p < kNumChParams; ++p) {
			pageIndices[ch][p] = (uint8_t)(ch * kNumChParams + p);
		}
		pageArray[ch].name = pageNames[ch];
		pageArray[ch].numParams = kNumChParams;
		pageArray[ch].params = pageIndices[ch];
	}
}

static _NT_parameterPages parameterPages;

// Specification: number of channels
static const _NT_specification specifications[] = {
	{ .name = "Channels", .min = 1, .max = 8, .def = 1, .type = kNT_typeGeneric },
};

// Per-channel gate state
struct ChannelState {
	bool gateHigh;
};

struct _triggerMidiAlgorithm : public _NT_algorithm {
	_triggerMidiAlgorithm() {}
	~_triggerMidiAlgorithm() {}

	int numChannels;
	ChannelState channels[kMaxChannels];
};

static uint32_t getDestination(int destParam) {
	switch (destParam) {
		case 0: return kNT_destinationBreakout;
		case 1: return kNT_destinationUSB;
		case 2: return kNT_destinationSelectBus;
		case 3: return kNT_destinationInternal;
		default: return kNT_destinationBreakout;
	}
}

static int parameterUiPrefix(_NT_algorithm* self, int p, char* buff) {
	_triggerMidiAlgorithm* pThis = (_triggerMidiAlgorithm*)self;
	if (pThis->numChannels <= 1)
		return 0;
	int ch = p / kNumChParams;
	int len = NT_intToString(buff, ch + 1);
	buff[len++] = ':';
	buff[len] = 0;
	return len;
}

static void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specs) {
	int numChannels = specs ? specs[0] : 1;
	if (numChannels < 1) numChannels = 1;
	if (numChannels > kMaxChannels) numChannels = kMaxChannels;

	req.numParameters = numChannels * kNumChParams;
	req.sram = sizeof(_triggerMidiAlgorithm);
	req.dram = 0;
	req.dtc = 0;
	req.itc = 0;
}

static _NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs,
                                const _NT_algorithmRequirements&,
                                const int32_t* specs) {
	int numChannels = specs ? specs[0] : 1;
	if (numChannels < 1) numChannels = 1;
	if (numChannels > kMaxChannels) numChannels = kMaxChannels;

	initParameters();
	initPages(numChannels);

	parameterPages.numPages = numChannels;
	parameterPages.pages = pageArray;

	_triggerMidiAlgorithm* alg = new (ptrs.sram) _triggerMidiAlgorithm();
	alg->numChannels = numChannels;
	alg->parameters = parameters;
	alg->parameterPages = &parameterPages;

	for (int ch = 0; ch < kMaxChannels; ++ch) {
		alg->channels[ch].gateHigh = false;
	}

	return alg;
}

static void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
	_triggerMidiAlgorithm* pThis = (_triggerMidiAlgorithm*)self;
	int numFrames = numFramesBy4 * 4;

	for (int ch = 0; ch < pThis->numChannels; ++ch) {
		int paramBase = ch * kNumChParams;
		int inputBus = pThis->v[paramBase + kChParamInput];

		// Skip if no input assigned (bus 0)
		if (inputBus == 0)
			continue;

		const float* in = busFrames + (inputBus - 1) * numFrames;
		int midiChannel = pThis->v[paramBase + kChParamMidiChannel] - 1; // 0-based
		uint32_t dest = getDestination(pThis->v[paramBase + kChParamMidiDest]);
		int note = pThis->v[paramBase + kChParamMidiNote];

		// Schmitt trigger: scan samples for rising/falling edges
		// High threshold to go high, low threshold to go low
		for (int i = 0; i < numFrames; ++i) {
			float sample = in[i];
			if (!pThis->channels[ch].gateHigh && sample >= kThresholdHigh) {
				// Rising edge - send note on
				NT_sendMidi3ByteMessage(dest, 0x90 | midiChannel, note, 127);
				pThis->channels[ch].gateHigh = true;
			} else if (pThis->channels[ch].gateHigh && sample <= kThresholdLow) {
				// Falling edge - send note off
				NT_sendMidi3ByteMessage(dest, 0x80 | midiChannel, note, 0);
				pThis->channels[ch].gateHigh = false;
			}
		}
	}
}

static const _NT_factory factory = {
	.guid = NT_MULTICHAR('T', 'h', 'T', 'm'),
	.name = "Trigger MIDI Notes",
	.description = "Convert triggers/gates to MIDI notes",
	.numSpecifications = ARRAY_SIZE(specifications),
	.specifications = specifications,
	.calculateStaticRequirements = NULL,
	.initialise = NULL,
	.calculateRequirements = calculateRequirements,
	.construct = construct,
	.parameterChanged = NULL,
	.step = step,
	.draw = NULL,
	.midiRealtime = NULL,
	.midiMessage = NULL,
	.tags = kNT_tagUtility,
	.hasCustomUi = NULL,
	.customUi = NULL,
	.setupUi = NULL,
	.serialise = NULL,
	.deserialise = NULL,
	.midiSysEx = NULL,
	.parameterUiPrefix = parameterUiPrefix,
	.parameterString = NULL,
};

uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
	switch (selector) {
	case kNT_selector_version:
		return kNT_apiVersionCurrent;
	case kNT_selector_numFactories:
		return 1;
	case kNT_selector_factoryInfo:
		return (uintptr_t)((data == 0) ? &factory : NULL);
	}
	return 0;
}
