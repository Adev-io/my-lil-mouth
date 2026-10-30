// Mouth.cpp: the Rack module. A microphone in; sung pitch, trained beatbox triggers, vowel and loudness out as CV.
// The DSP lives in core/ (Yin, NoteLock, Env, Onset, Features, TrigBank, Formant); this file buffers the input,
// runs the analysis once per hop and turns the results into voltages, lights and patch data.
// Author: Ari Klopfer / A_Development. MIT
#include "plugin.hpp"
#include "layout.hpp"
#include "Env.h"
#include "Features.h"
#include "Fft.h"
#include "Formant.h"
#include "NoteLock.h"
#include "Onset.h"
#include "TrigBank.h"
#include "Yin.h"
#include "TalkingFace.hpp"
#include <atomic>

static const float RANGE_MIN[] = { 50.f, 70.f, 150.f };
static const float RANGE_MAX[] = { 500.f, 1100.f, 2000.f };
static const float PULSE_MS[] = { 5.f, 1.f };

struct Mouth : Module
{
    enum ParamId { GAIN_PARAM, THRESH_PARAM, CAL_PARAM, LOCK_PARAM, GLIDE_PARAM, ENUMS (TRAIN_PARAM, 4), SENS_PARAM, PARAMS_LEN };
    enum InputId { IN_INPUT, INPUTS_LEN };
    enum OutputId { NOTE_OUTPUT, RAW_OUTPUT, GATE_OUTPUT, VEL_OUTPUT, ENUMS (TRIG_OUTPUT, 4), X_OUTPUT, Y_OUTPUT, BRIGHT_OUTPUT, ENV_OUTPUT, OUTPUTS_LEN };
    enum LightId { ENUMS (CAL_LIGHT, 2), ENUMS (NOTE_LIGHT, 12), ENUMS (TRAIN_LIGHT, 4 * 3), LIGHTS_LEN };

    Yin yin;
    NoteLock lock;
    Env env;
    Onset onset;
    TrigBank bank;
    Formant formant;
    FaceTap face;   // the mascot's mouth: the input after GAIN

    // saved with the patch
    int range = 1;            // index into RANGE_MIN / RANGE_MAX: Voice
    int pulse = 0;            // index into PULSE_MS
    float floorDb = -60.f;    // noise floor from CAL, measured after GAIN
    float calGainDb = 0.f;    // GAIN when CAL measured: the threshold moves with GAIN changes made since
    bool calibrated = false;

    // buffers, sized in onSampleRateChange
    std::vector<float> ring, pitchBuf, frame, hann, hannHalf;
    std::vector<std::complex<double>> spec, specHalf;
    int head = 0, mask = 0, window = 1024, hop = 256, hopCount = 0;
    float hopEnergy = 0.f;
    float sr = 48000.f;
    int appliedRange = -1;

    // input filter
    float hpCoef = 0.99f, hpX = 0.f, hpY = 0.f;

    // pitch and gate
    bool gate = false;
    float gateLowSec = 0.f, retrigSec = 0.f, vel = 0.f, rawV = 0.f;

    // triggers
    int pending = 0;          // hops until the onset's features are taken
    int training = -1;        // slot whose TRAIN is held
    std::atomic<bool> resetTraining { false };   // set by the menu (UI thread), applied in process()
    float trainHeldSec = 0.f;
    dsp::PulseGenerator trig[4];
    float flash[4] = {};
    dsp::BooleanTrigger trainBtn[4], calBtn;

    // vowel and brightness, smoothed per sample
    float xT = 0.f, yT = 0.f, xV = 0.f, yV = 0.f, bright = 0.f, smooth = 0.001f;

    // calibration
    float calSec = 0.f, calSum = 0.f;
    int calN = 0;

    dsp::ClockDivider lightDiv;
    float blink = 0.f;

    Mouth()
    {
        config (PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam (GAIN_PARAM, 0.f, 40.f, 0.f, "Input gain", " dB");
        configParam (THRESH_PARAM, 0.f, 40.f, 12.f, "Threshold above the noise floor", " dB");
        configButton (CAL_PARAM, "Calibrate the noise floor (2 s of room sound)");
        configParam (LOCK_PARAM, 50.f, 100.f, 60.f, "Note lock (50 = nearest note, 100 = only on reaching the next)", " cents");
        configParam (GLIDE_PARAM, 0.f, 200.f, 0.f, "Glide", " ms");
        for (int i = 0; i < 4; ++i) configButton (TRAIN_PARAM + i, "Train trigger " + std::to_string (i + 1) + " (hold and make the sound 3 or more times; hold 3 s silent to clear)");
        configParam (SENS_PARAM, 0.f, 1.f, 0.5f, "Trigger sensitivity", " %", 0.f, 100.f);
        configInput (IN_INPUT, "Microphone");
        configOutput (NOTE_OUTPUT, "Note (V/Oct, held with note lock)");
        configOutput (RAW_OUTPUT, "Raw pitch (V/Oct, follows the voice)");
        configOutput (GATE_OUTPUT, "Gate (10 V while singing)");
        configOutput (VEL_OUTPUT, "Velocity (0-10 V, loudness at the note start)");
        for (int i = 0; i < 4; ++i) configOutput (TRIG_OUTPUT + i, "Trigger " + std::to_string (i + 1));
        configOutput (X_OUTPUT, "Vowel X (F1: ee/oo low, ah high)");
        configOutput (Y_OUTPUT, "Vowel Y (F2: oo low, ee high)");
        configOutput (BRIGHT_OUTPUT, "Brightness (spectral centroid)");
        configOutput (ENV_OUTPUT, "Envelope (0-10 V)");
        lightDiv.setDivision (64);
        resize (APP->engine->getSampleRate());
    }

    void onSampleRateChange (const SampleRateChangeEvent& e) override { resize (e.sampleRate); }

    void resize (float sampleRate)
    {
        sr = sampleRate;
        window = 1 << (int) std::lround (std::log2 (0.0213f * sr));   // the power of two nearest 21.3 ms
        hop = window / 4;
        yin.set (RANGE_MIN[0], RANGE_MAX[0], sr);   // the largest pitch buffer first, so a range change never allocates
        pitchBuf.assign (yin.length(), 0.f);
        int n = 1;
        while (n < std::max (window, yin.length())) n <<= 1;
        ring.assign (n, 0.f);
        mask = n - 1;
        head = hopCount = 0;
        hopEnergy = 0.f;
        frame.assign (window, 0.f);
        spec.assign (window, 0.0);
        specHalf.assign (window / 2, 0.0);
        hann.resize (window);
        hannHalf.resize (window / 2);
        for (int i = 0; i < window; ++i) hann[i] = 0.5f - 0.5f * std::cos (2.f * M_PI * i / (window - 1));
        for (int i = 0; i < window / 2; ++i) hannHalf[i] = 0.5f - 0.5f * std::cos (2.f * M_PI * i / (window / 2 - 1));
        hpCoef = std::exp (-2.f * M_PI * 80.f / sr);
        env.set (5.f, 50.f, sr);
        formant.set (sr);
        smooth = 1.f - std::exp (-1.f / (0.020f * sr));
        appliedRange = -1;   // process() sets the chosen range
    }

    void onReset (const ResetEvent& e) override
    {
        Module::onReset (e);
        if (training >= 0) bank.endTraining();
        training = -1;
        for (int s = 0; s < 4; ++s) bank.clear (s);
        floorDb = -60.f;
        calGainDb = 0.f;
        calibrated = false;
        range = 1;
        pulse = 0;
    }

    // The last n samples, oldest first.
    void last (int n, float* dst) const
    {
        const int start = head - n;
        for (int i = 0; i < n; ++i) dst[i] = ring[(start + i) & mask];
    }

    float floorNowDb() { return floorDb + params[GAIN_PARAM].getValue() - calGainDb; }   // the floor at the current GAIN
    float thresholdLin() { return std::pow (10.f, (floorNowDb() + params[THRESH_PARAM].getValue()) / 20.f); }

    void hitOrTrain()
    {
        const int n = window / 2;
        last (n, frame.data());
        for (int i = 0; i < n; ++i) specHalf[i] = frame[i] * hannHalf[i];
        voice::fft (specHalf);
        Features f;
        f.compute (specHalf, sr);
        if (training >= 0)
        {
            bank.train (f.vec);
            flash[training] = 0.06f;
            return;
        }
        const int s = bank.classify (f.vec);
        if (s < 0) return;
        trig[s].trigger (PULSE_MS[pulse] / 1000.f);
        flash[s] = 0.06f;
    }

    void analyse()
    {
        const float hopSec = hop / sr, thr = thresholdLin();
        const bool above = env.level > thr;

        // pitch, gate, velocity
        Yin::Result r;
        r.f0 = r.confidence = 0.f;
        if (above)   // silence costs nothing (YIN is the module's heaviest step)
        {
            last (yin.length(), pitchBuf.data());
            r = yin.process (pitchBuf.data());
        }
        const bool voiced = r.f0 > 0.f && above;
        if (voiced)
        {
            const bool starting = ! gate;
            gate = true;
            gateLowSec = 0.f;
            const NoteLock::Out o = lock.process (r.f0, true);
            rawV = std::log2 (r.f0 / 261.6256f);
            if (starting || o.changed) vel = std::min (10.f, 10.f * env.level);
            if (o.changed && ! starting) retrigSec = 0.002f;
        }
        else if (gate && (gateLowSec += hopSec) >= 0.030f)
        {
            gate = false;
            lock.process (0.f, false);
        }

        // triggers: the features are taken 1 hop after the onset, so the half-window spans the onset hop (where the hit
        // starts) and the hop after it
        if (pending > 0 && --pending == 0) hitOrTrain();
        onset.set (params[SENS_PARAM].getValue(), 30.f, hopSec);
        if (onset.process (hopEnergy / hop, thr * thr)) pending = 1;

        // brightness and vowel, held while the input is under the threshold
        if (! above) return;
        last (window, frame.data());
        for (int i = 0; i < window; ++i) spec[i] = frame[i] * hann[i];
        voice::fft (spec);
        Features f;
        f.compute (spec, sr);
        bright = 10.f * f.vec[Features::BANDS];
        const Formant::Result fr = formant.process (frame.data(), window, voiced ? r.f0 : 0.f);
        if (fr.ok)
        {
            xT = clamp ((fr.f1 - 250.f) / (900.f - 250.f) * 10.f, 0.f, 10.f);
            yT = clamp ((fr.f2 - 600.f) / (3000.f - 600.f) * 10.f, 0.f, 10.f);
        }
    }

    void process (const ProcessArgs& args) override
    {
        if (appliedRange != range)
        {
            yin.set (RANGE_MIN[range], RANGE_MAX[range], sr);   // within the capacity sized in onSampleRateChange
            appliedRange = range;
        }
        lock.set (params[LOCK_PARAM].getValue(), params[GLIDE_PARAM].getValue());

        float x = inputs[IN_INPUT].getVoltage() / 10.f * std::pow (10.f, params[GAIN_PARAM].getValue() / 20.f);
        if (! std::isfinite (x)) x = 0.f;
        hpY = hpCoef * (hpY + x - hpX);   // one-pole high-pass at 80 Hz
        hpX = x;
        ring[head] = hpY;
        head = (head + 1) & mask;
        env.process (hpY);
        face.push (clamp (hpY, -1.f, 1.f), args.sampleRate);

        // calibration: mean square over 2 s
        if (calBtn.process (params[CAL_PARAM].getValue() > 0.f) && calSec <= 0.f)
        {
            calSec = 2.f;
            calSum = 0.f;
            calN = 0;
        }
        if (calSec > 0.f)
        {
            calSum += hpY * hpY;
            ++calN;
            if ((calSec -= args.sampleTime) <= 0.f)
            {
                floorDb = clamp (10.f * std::log10 (calSum / std::max (1, calN) + 1e-12f), -100.f, -10.f);
                calGainDb = params[GAIN_PARAM].getValue();
                calibrated = true;
            }
        }

        if (resetTraining.exchange (false))
        {
            if (training >= 0) bank.endTraining();   // end first, so the slot's backup cannot bring it back
            training = -1;
            for (int s = 0; s < 4; ++s) bank.clear (s);
        }

        // training: one slot at a time
        for (int s = 0; s < 4; ++s)
        {
            const bool held = params[TRAIN_PARAM + s].getValue() > 0.f;
            if (trainBtn[s].process (held) && training < 0)
            {
                training = s;
                trainHeldSec = 0.f;
                bank.beginTraining (s);
            }
            if (training == s && ! held)
            {
                const bool wipe = trainHeldSec >= 3.f && bank.examples() == 0;
                bank.endTraining();
                if (wipe) bank.clear (s);
                training = -1;
            }
        }
        if (training >= 0) trainHeldSec += args.sampleTime;

        hopEnergy += hpY * hpY;
        if (++hopCount >= hop)
        {
            analyse();
            hopCount = 0;
            hopEnergy = 0.f;
        }

        if (retrigSec > 0.f) retrigSec -= args.sampleTime;
        xV += smooth * (xT - xV);
        yV += smooth * (yT - yV);
        outputs[NOTE_OUTPUT].setVoltage (lock.step (args.sampleTime));
        outputs[RAW_OUTPUT].setVoltage (rawV);
        outputs[GATE_OUTPUT].setVoltage (gate && retrigSec <= 0.f ? 10.f : 0.f);
        outputs[VEL_OUTPUT].setVoltage (vel);
        for (int s = 0; s < 4; ++s) outputs[TRIG_OUTPUT + s].setVoltage (trig[s].process (args.sampleTime) ? 10.f : 0.f);
        outputs[X_OUTPUT].setVoltage (xV);
        outputs[Y_OUTPUT].setVoltage (yV);
        outputs[BRIGHT_OUTPUT].setVoltage (bright);
        outputs[ENV_OUTPUT].setVoltage (std::min (10.f, 10.f * env.level));

        if (lightDiv.process())
        {
            const float dt = args.sampleTime * lightDiv.getDivision();
            blink = std::fmod (blink + dt, 0.4f);
            const bool on = blink < 0.2f;
            lights[CAL_LIGHT + 0].setBrightness (calSec <= 0.f && calibrated ? 1.f : 0.f);
            lights[CAL_LIGHT + 1].setBrightness (calSec > 0.f && on ? 1.f : 0.f);
            const int pc = ((lock.note() % 12) + 12) % 12;
            for (int k = 0; k < 12; ++k) lights[NOTE_LIGHT + k].setBrightness (k != pc ? 0.f : gate ? 1.f : 0.25f);
            for (int s = 0; s < 4; ++s)
            {
                flash[s] = std::max (0.f, flash[s] - dt);
                const bool hit = flash[s] > 0.f, learning = training == s;
                lights[TRAIN_LIGHT + 3 * s + 0].setBrightness (! hit && ! learning && bank.slots[s].trained() ? 1.f : 0.f);
                lights[TRAIN_LIGHT + 3 * s + 1].setBrightness (! hit && learning && on ? 1.f : 0.f);
                lights[TRAIN_LIGHT + 3 * s + 2].setBrightness (hit ? 1.f : 0.f);
            }
        }
    }

    json_t* dataToJson() override
    {
        json_t* root = json_object();
        json_object_set_new (root, "triggers", json_string (bank.save().c_str()));
        json_object_set_new (root, "floorDb", json_real (floorDb));
        json_object_set_new (root, "calGainDb", json_real (calGainDb));
        json_object_set_new (root, "calibrated", json_boolean (calibrated));
        json_object_set_new (root, "range", json_integer (range));
        json_object_set_new (root, "pulse", json_integer (pulse));
        return root;
    }

    void dataFromJson (json_t* root) override
    {
        json_t* t = json_object_get (root, "triggers");
        if (json_is_string (t)) bank.load (json_string_value (t));   // malformed: the bank stays as it is (Rack holds the engine lock here)
        json_t* f = json_object_get (root, "floorDb");
        if (json_is_number (f) && std::isfinite (json_number_value (f))) floorDb = clamp ((float) json_number_value (f), -100.f, -10.f);
        json_t* g = json_object_get (root, "calGainDb");
        if (json_is_number (g) && std::isfinite (json_number_value (g))) calGainDb = clamp ((float) json_number_value (g), 0.f, 40.f);
        json_t* c = json_object_get (root, "calibrated");
        if (json_is_boolean (c)) calibrated = json_boolean_value (c);
        json_t* r = json_object_get (root, "range");
        if (json_is_integer (r)) range = clamp ((int) json_integer_value (r), 0, 2);
        json_t* p = json_object_get (root, "pulse");
        if (json_is_integer (p)) pulse = clamp ((int) json_integer_value (p), 0, 1);
    }
};

struct MouthWidget : ModuleWidget
{
    MouthWidget (Mouth* module)
    {
        setModule (module);
        setPanel (createPanel (asset::plugin (pluginInstance, "res/Mouth.svg")));
        namespace L = layout::mouth;
        {
            TalkingFace* f = createWidget<TalkingFace> (mm2px (Vec (L::FACE[0] - L::FACE[2], L::FACE[1] - L::FACE[2])));
            f->box.size = mm2px (Vec (2 * L::FACE[2], 2 * L::FACE[2]));
            if (module) f->tap = &module->face;
            addChild (f);
        }
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto at = [] (const float* mm) { return mm2px (Vec (mm[0], mm[1])); };
        addParam (createParamCentered<Rogan1PSWhite> (at (L::GAIN_PARAM), module, Mouth::GAIN_PARAM));
        addParam (createParamCentered<Rogan1PSWhite> (at (L::THRESH_PARAM), module, Mouth::THRESH_PARAM));
        addParam (createLightParamCentered<VCVLightButton<MediumSimpleLight<LimeMagentaLight>>> (at (L::CAL_PARAM), module, Mouth::CAL_PARAM, Mouth::CAL_LIGHT));
        addParam (createParamCentered<Rogan1PSWhite> (at (L::LOCK_PARAM), module, Mouth::LOCK_PARAM));
        addParam (createParamCentered<Rogan1PSWhite> (at (L::GLIDE_PARAM), module, Mouth::GLIDE_PARAM));
        static const float* const notes[12] = { L::NOTE0_LIGHT, L::NOTE1_LIGHT, L::NOTE2_LIGHT, L::NOTE3_LIGHT, L::NOTE4_LIGHT, L::NOTE5_LIGHT,
                                                L::NOTE6_LIGHT, L::NOTE7_LIGHT, L::NOTE8_LIGHT, L::NOTE9_LIGHT, L::NOTE10_LIGHT, L::NOTE11_LIGHT };
        for (int k = 0; k < 12; ++k) addChild (createLightCentered<MediumLight<LimeLight>> (at (notes[k]), module, Mouth::NOTE_LIGHT + k));
        static const float* const trains[4] = { L::TRAIN0_PARAM, L::TRAIN1_PARAM, L::TRAIN2_PARAM, L::TRAIN3_PARAM };
        for (int s = 0; s < 4; ++s)
            addParam (createLightParamCentered<VCVLightBezel<LimeMagentaWhiteLight>> (at (trains[s]), module, Mouth::TRAIN_PARAM + s, Mouth::TRAIN_LIGHT + 3 * s));
        addParam (createParamCentered<Rogan1PSWhite> (at (L::SENS_PARAM), module, Mouth::SENS_PARAM));
        addInput (createInputCentered<PJ301MPort> (at (L::IN_INPUT), module, Mouth::IN_INPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::NOTE_OUTPUT), module, Mouth::NOTE_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::RAW_OUTPUT), module, Mouth::RAW_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::GATE_OUTPUT), module, Mouth::GATE_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::VEL_OUTPUT), module, Mouth::VEL_OUTPUT));
        static const float* const trigs[4] = { L::TRIG0_OUTPUT, L::TRIG1_OUTPUT, L::TRIG2_OUTPUT, L::TRIG3_OUTPUT };
        for (int s = 0; s < 4; ++s) addOutput (createOutputCentered<PJ301MPort> (at (trigs[s]), module, Mouth::TRIG_OUTPUT + s));
        addOutput (createOutputCentered<PJ301MPort> (at (L::X_OUTPUT), module, Mouth::X_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::Y_OUTPUT), module, Mouth::Y_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::BRIGHT_OUTPUT), module, Mouth::BRIGHT_OUTPUT));
        addOutput (createOutputCentered<PJ301MPort> (at (L::ENV_OUTPUT), module, Mouth::ENV_OUTPUT));
    }

    void appendContextMenu (Menu* menu) override
    {
        Mouth* m = dynamic_cast<Mouth*> (module);
        if (! m) return;
        menu->addChild (new MenuSeparator);
        menu->addChild (createIndexPtrSubmenuItem ("Pitch range", { "Bass (50-500 Hz)", "Voice (70-1100 Hz)", "High (150-2000 Hz)" }, &m->range));
        menu->addChild (createIndexPtrSubmenuItem ("Trigger pulse", { "5 ms", "1 ms" }, &m->pulse));
        menu->addChild (createMenuItem ("Reset all training", "", [=] { m->resetTraining = true; }));
        menu->addChild (createMenuLabel (string::f ("Noise floor at this GAIN: %.0f dBFS%s", m->floorNowDb(), m->calibrated ? "" : " (press CAL)")));
    }
};

Model* modelMouth = createModel<Mouth, MouthWidget> ("Mouth");
