#pragma once

#include <mw/models/tx/Transaction.h>
#include <mw/models/wallet/Coin.h>
#include <mw/models/wallet/StealthAddress.h>
#include <optional>

MW_NAMESPACE

struct MutableInput {
    // UTXO: public info
    mw::Hash output_id;
    std::optional<Commitment> commitment;
    std::optional<PublicKey> output_pubkey;

    // UTXO: secrets
    std::optional<CAmount> amount; //! The amount being spent.
    std::optional<SecretKey> spend_key; //! Secret key needed to spend the UTXO.
    std::optional<BlindingFactor> raw_blind; //! Pre-switch blinding factor.

    // Input: public info
    std::optional<uint8_t> features;
    std::optional<PublicKey> input_pubkey;
    std::vector<uint8_t> extradata;
    std::optional<Signature> signature;

    // Input: secrets
    std::optional<SecretKey> ephemeral_key; //! Secret key of the input_pubkey. Generated when signing.

    MutableInput(mw::Hash output_id_)
        : output_id(std::move(output_id_)) { }

    // Builds a MutableInput with the public UTXO fields populated for spending the provided output.
    static MutableInput FromOutput(const mw::Output& output)
    {
        MutableInput input(output.GetOutputID());
        input.commitment = output.GetCommitment();
        input.output_pubkey = output.GetReceiverPubKey();
        return input;
    }

    static MutableInput FromCoin(const mw::Coin& coin)
    {
        MutableInput input(coin.output_id);
        input.amount = coin.amount;
        input.raw_blind = coin.blind;
        if (input.raw_blind.has_value()) {
            input.commitment = Commitment::Switch(*input.raw_blind, coin.amount);
        }
        input.spend_key = coin.spend_key;
        if (input.spend_key.has_value()) {
            input.output_pubkey = PublicKey::From(*input.spend_key);
        }
        return input;
    }

    bool operator==(const MutableInput& other) const noexcept
    {
        return output_id == other.output_id &&
               features == other.features &&
               commitment == other.commitment &&
               input_pubkey == other.input_pubkey &&
               output_pubkey == other.output_pubkey &&
               extradata == other.extradata &&
               signature == other.signature &&
               amount == other.amount &&
               spend_key == other.spend_key &&
               raw_blind == other.raw_blind &&
               ephemeral_key == other.ephemeral_key;
    }

    // A finalized MutableInput is one that has all public fields populated
    bool IsFinal() const noexcept
    {
        return commitment.has_value() &&
            output_pubkey.has_value() &&
            features.has_value() &&
            input_pubkey.has_value() &&
            signature.has_value();
    }

    // Returns the finalized (signed) mw::Input. Will be nullopt if input is not final.
    std::optional<Input> Finalized() const noexcept
    {
        if (IsFinal()) {
            return Input(*features, output_id, *commitment, *input_pubkey, *output_pubkey, *signature); // MW: TODO - Include extradata
        }

        return std::nullopt;
    }

    // Updates the public fields of the MutableInput to their finalized values.
    void Update(const Input& finalized) noexcept
    {
        features = finalized.GetFeatures();
        output_id = finalized.GetOutputID();
        commitment = finalized.GetCommitment();
        input_pubkey = finalized.GetInputPubKey();
        output_pubkey = finalized.GetOutputPubKey();
        extradata = finalized.GetExtraData();
        signature = finalized.GetSignature();
    }
};

struct MutableOutput {
    // MW: TODO: Could probably just be an optional Output and optional Recipient
    std::optional<Commitment> commitment{};
    std::optional<PublicKey> sender_pubkey{};
    std::optional<PublicKey> receiver_pubkey{};
    std::optional<mw::OutputMessage> message{};
    std::optional<RangeProof::CPtr> proof{};
    std::optional<Signature> signature{};

    /*
    std::optional<uint8_t> features;
    std::optional<PublicKey> key_exchange_pubkey;
    std::optional<uint8_t> view_tag;
    std::optional<uint64_t> masked_value;
    std::optional<BigInt<16>> masked_nonce;
    std::vector<uint8_t> extra_data;
    */

    std::optional<CAmount> amount{};
    std::optional<StealthAddress> address{};
    std::optional<bool> subtract_fee_from_amount{};
    // MW: TODO - Include generated secrets

    bool operator==(const MutableOutput& other) const noexcept
    {
        return commitment == other.commitment &&
               sender_pubkey == other.sender_pubkey &&
               receiver_pubkey == other.receiver_pubkey &&
               message == other.message &&
               proof == other.proof &&
               signature == other.signature &&
               amount == other.amount &&
               address == other.address &&
               subtract_fee_from_amount == other.subtract_fee_from_amount;
    }

    // A finalized MutableOutput is one that has all public fields populated
    bool IsFinal() const noexcept
    {
        return commitment.has_value() &&
               sender_pubkey.has_value() &&
               receiver_pubkey.has_value() &&
               message.has_value() &&
               proof.has_value() &&
               signature.has_value();
    }

    // Returns the finalized (signed) mw::Output. Will be nullopt if output is not final.
    std::optional<mw::Output> Finalized() const noexcept
    {
        if (IsFinal()) {
            return mw::Output(*commitment, *sender_pubkey, *receiver_pubkey, *message, *proof, *signature);
        }

        return std::nullopt;
    }

    std::optional<mw::Hash> CalcOutputID() const noexcept
    {
        std::optional<mw::Output> output = Finalized();
        return output.has_value() ? std::make_optional(output->GetOutputID()) : std::nullopt;
    }

    // Updates the public fields of the MutableOutput to their finalized values.
    void Update(const mw::Output& finalized) noexcept
    {
        commitment = finalized.GetCommitment();
        sender_pubkey = finalized.GetSenderPubKey();
        receiver_pubkey = finalized.GetReceiverPubKey();
        message = finalized.GetOutputMessage();
        proof = finalized.GetRangeProof();
        signature = finalized.GetSignature();
    }
};

struct PegOutRecipient
{
    CScript script;
    CAmount nAmount;
    bool fSubtractFeeFromAmount;

    bool operator==(const PegOutRecipient& rhs) const noexcept
    {
        return script == rhs.script && nAmount == rhs.nAmount && fSubtractFeeFromAmount == rhs.fSubtractFeeFromAmount;
    }
};

struct MutableKernel {
    std::optional<CAmount> fee;
    std::optional<CAmount> pegin;
    std::vector<PegOutRecipient> pegouts;
    std::optional<int32_t> lock_height;
    std::optional<PublicKey> stealth_excess;
    std::vector<uint8_t> extradata;
    std::optional<Commitment> excess;
    std::optional<Signature> signature;

    // MW: TODO - Should include secret keys

    bool operator==(const MutableKernel& other) const noexcept
    {
        return fee == other.fee &&
               pegin == other.pegin &&
               pegouts == other.pegouts &&
               lock_height == other.lock_height &&
               stealth_excess == other.stealth_excess &&
               extradata == other.extradata &&
               excess == other.excess &&
               signature == other.signature;
    }

    // A finalized MutableKernel is one that has all public fields populated
    bool IsFinal() const noexcept
    {
        return excess.has_value() && signature.has_value();
    }

    uint8_t CalcFeatureByte() const noexcept
    {
        uint8_t features = 0;
        features |= fee.has_value() ? mw::Kernel::FEE_FEATURE_BIT : 0;
        features |= pegin.has_value() ? mw::Kernel::PEGIN_FEATURE_BIT : 0;
        features |= pegouts.size() > 0 ? mw::Kernel::PEGOUT_FEATURE_BIT : 0;
        features |= lock_height.has_value() ? mw::Kernel::HEIGHT_LOCK_FEATURE_BIT : 0;
        features |= stealth_excess.has_value() ? mw::Kernel::STEALTH_EXCESS_FEATURE_BIT : 0;
        features |= extradata.size() > 0 ? mw::Kernel::EXTRA_DATA_FEATURE_BIT : 0;
        return features;
    }

    // Returns the finalized (signed) mw::Kernel. Will be nullopt if kernel is not final.
    std::optional<mw::Kernel> Finalized() const noexcept
    {
        if (IsFinal()) {
            return mw::Kernel(CalcFeatureByte(), fee, pegin, GetPegOuts(), lock_height, stealth_excess, extradata, *excess, *signature);
        }

        return std::nullopt;
    }

    void SetPegOuts(const std::vector<PegOutCoin>& pegout_coins) {
        pegouts.clear();
        for (const PegOutCoin& pegout_coin : pegout_coins) {
            pegouts.push_back(PegOutRecipient{pegout_coin.GetScriptPubKey(), pegout_coin.GetAmount(), false});
        }
    }

    // Updates the public fields of the MutableKernel to their finalized values.
    void Update(const mw::Kernel& finalized) noexcept
    {
        fee = finalized.m_fee;
        pegin = finalized.m_pegin;
        SetPegOuts(finalized.m_pegouts);
        lock_height = finalized.m_lockHeight;
        stealth_excess = finalized.m_stealthExcess;
        extradata = finalized.m_extraData;
        excess = finalized.m_excess;
        signature = finalized.m_signature;
    }

    // Calculates the kernel ID. Will be nullopt if kernel is not final
    std::optional<mw::Hash> GetKernelID() const noexcept
    {
        std::optional<mw::Kernel> kernel = Finalized();
        if (kernel.has_value()) {
            return kernel->GetKernelID();
        }

        return std::nullopt;
    }

    std::vector<PegOutCoin> GetPegOuts() const noexcept
    {
        std::vector<PegOutCoin> pegouts_;
        for (const PegOutRecipient& pegout : pegouts) {
            pegouts_.push_back(PegOutCoin{pegout.nAmount, pegout.script});
        }
        return pegouts_;
    }
};

struct MutableTx : public Traits::ISerializable
{
    BlindingFactor kernel_offset;
    BlindingFactor stealth_offset;
    std::vector<MutableInput> inputs;
    std::vector<MutableOutput> outputs;
    std::vector<MutableKernel> kernels; // MW: TODO - Just store fee, pegins, and pegouts here, rather than having mutable kernels?

    static MutableTx From(const mw::Transaction& tx)
    {
        MutableTx mutable_tx{};
        mutable_tx.kernel_offset = tx.GetKernelOffset();
        mutable_tx.stealth_offset = tx.GetStealthOffset();

        for (const Input& input : tx.GetInputs()) {
            MutableInput mutable_input(input.GetOutputID());
            mutable_input.Update(input);
            mutable_tx.inputs.push_back(std::move(mutable_input));
        }

        for (const Output& output : tx.GetOutputs()) {
            MutableOutput mutable_output{};
            mutable_output.Update(output);
            mutable_tx.outputs.push_back(std::move(mutable_output));
        }

        for (const Kernel& kernel : tx.GetKernels()) {
            MutableKernel mutable_kernel{};
            mutable_kernel.Update(kernel);
            mutable_tx.kernels.push_back(std::move(mutable_kernel));
        }

        return mutable_tx;
    }

    void SetFee(const CAmount fee) noexcept
    {
        if (kernels.empty()) {
            kernels.push_back(mw::MutableKernel{});
        }

        kernels.front().fee = fee;
    }

    std::optional<CAmount> GetPeginAmount() const noexcept
    {
        if (kernels.empty()) {
            return std::nullopt;
        }

        return kernels.front().pegin;
    }

    void SetPeginAmount(const CAmount pegin_amount)
    {
        if (kernels.empty()) {
            kernels.push_back(mw::MutableKernel{});
        }

        kernels.front().pegin = pegin_amount;
    }

    void AddPegout(const PegOutRecipient& pegout_recipient)
    {
        if (kernels.empty()) {
            kernels.push_back(mw::MutableKernel{});
        }

        kernels.front().pegouts.push_back(pegout_recipient);
    }

    void AddPegout(CScript script, const CAmount amount, bool subtract_fee_from_amount)
    {
        AddPegout(PegOutRecipient{std::move(script), amount, subtract_fee_from_amount});
    }

    std::vector<PegOutRecipient> GetPegouts() const noexcept
    {
        std::vector<PegOutRecipient> pegouts;
        for (const MutableKernel& kernel : kernels) {
            pegouts.insert(pegouts.end(), kernel.pegouts.begin(), kernel.pegouts.end());
        }

        return pegouts;
    }

    CAmount GetTotalPegoutAmount() const noexcept
    {
        CAmount pegout_amount = 0;
        for (const MutableKernel& kernel : kernels) {
            for (const mw::PegOutRecipient& pegout : kernel.pegouts) {
                pegout_amount += pegout.nAmount;
            }
        }

        return pegout_amount;
    }

    bool operator==(const MutableTx& tx) const noexcept
    {
        return kernel_offset == tx.kernel_offset && stealth_offset == tx.stealth_offset && inputs == tx.inputs && outputs == tx.outputs && kernels == tx.kernels;
    }

    bool IsNull() const noexcept
    {
        return kernel_offset.IsZero() && stealth_offset.IsZero() && inputs.empty() && outputs.empty() && kernels.empty();
    }

    void SetNull() noexcept
    {
        kernel_offset = BlindingFactor{};
        stealth_offset = BlindingFactor{};
        inputs.clear();
        outputs.clear();
        kernels.clear();
    }

    bool IsFinal() const noexcept
    {
        if (IsNull()) {
            return true;
        }

        if (kernels.empty()) {
            return false;
        }

        for (const mw::MutableInput& input : inputs) {
            if (!input.IsFinal()) {
                return false;
            }
        }

        for (const mw::MutableOutput& output : outputs) {
            if (!output.IsFinal()) {
                return false;
            }
        }

        for (const mw::MutableKernel& kernel : kernels) {
            if (!kernel.IsFinal()) {
                return false;
            }
        }

        return true;
    }

    std::optional<mw::Transaction::CPtr> Finalized() const noexcept
    {
        if (IsNull() || !IsFinal()) {
            return std::nullopt;
        }

        std::vector<Input> final_inputs;
        for (const mw::MutableInput& input : inputs) {
            final_inputs.push_back(*input.Finalized());
        }

        std::vector<Output> final_outputs;
        for (const mw::MutableOutput& output : outputs) {
            final_outputs.push_back(*output.Finalized());
        }

        std::vector<Kernel> final_kernels;
        for (const mw::MutableKernel& kernel : kernels) {
            final_kernels.push_back(*kernel.Finalized());
        }

        return mw::Transaction::Create(
            kernel_offset,
            stealth_offset,
            std::move(final_inputs),
            std::move(final_outputs),
            std::move(final_kernels)
        );
    }

    std::vector<PegInCoin> GetPegIns() const noexcept
    {
        std::vector<PegInCoin> pegins;
        for (const MutableKernel& kernel : kernels) {
            if (kernel.pegin.has_value()) {
                mw::Hash kernel_id = kernel.GetKernelID().value_or(mw::Hash{});
                pegins.push_back(PegInCoin{kernel.pegin.value(), kernel_id});
            }
        }

        return pegins;
    }

    std::vector<PegOutCoin> GetPegOutCoins() const noexcept
    {
        std::vector<PegOutCoin> pegout_coins;
        for (const MutableKernel& kernel : kernels) {
            for (const PegOutRecipient& pegout : kernel.pegouts) {
                pegout_coins.push_back(PegOutCoin{pegout.nAmount, pegout.script});
            }
        }

        return pegout_coins;
    }

    uint32_t GetMWEBWeight() const noexcept
    {
        if (inputs.empty() && outputs.empty() && kernels.empty()) {
            return 0;
        }

        return (mw::STANDARD_OUTPUT_WEIGHT * outputs.size()) + Weight::CalcKernelWeight(true, GetPegOutCoins());
    }

    //
    // Serialization/Deserialization
    //
    IMPL_SERIALIZABLE(MutableTx, obj)
    {
        mw::Transaction::CPtr tx;
        SER_WRITE(obj, tx = obj.Finalized().value_or(nullptr));
        READWRITE(WrapOptionalPtr(tx));
        SER_READ(obj, if (tx) obj = MutableTx::From(*tx) );
    }
};

END_NAMESPACE
