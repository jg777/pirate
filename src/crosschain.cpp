#include "cc/eval.h"
#include "importcoin.h"
#include "main.h"
#include "notarisationdb.h"
#include "komodo_structs.h"


/*
 * This file is built in the server
 */

/* On KMD */
uint256 CalculateProofRoot(const char* symbol, uint32_t targetCCid, int kmdHeight,
        std::vector<uint256> &moms, uint256 &destNotarisationTxid)
{
    /*
     * Notaries don't wait for confirmation on KMD before performing a backnotarisation,
     * but we need a determinable range that will encompass all merkle roots. Include MoMs
     * including the block height of the last notarisation until the height before the
     * previous notarisation.
     *
     *    kmdHeight      notarisations-0      notarisations-1
     *        |                |********************|
     *        > scan backwards >
     */

    if (targetCCid <= 1)
        return uint256();

    if (kmdHeight < 0 || kmdHeight > chainActive.Height())
        return uint256();

    int seenOwnNotarisations = 0;

    for (int i=0; i<1440; i++) {
        if (i > kmdHeight) break;
        NotarisationsInBlock notarisations;
        uint256 blockHash = *chainActive[kmdHeight-i]->phashBlock;
        if (!GetBlockNotarisations(blockHash, notarisations))
            continue;
        BOOST_FOREACH(Notarisation& nota, notarisations) {
            NotarisationData& data = nota.second;
            if (data.ccId != targetCCid)
                continue;
            if (strcmp(data.symbol, symbol) == 0)
            {
                seenOwnNotarisations++;
                if (seenOwnNotarisations == 2)
                    goto end;
                if (seenOwnNotarisations == 1)
                    destNotarisationTxid = nota.first;
            }
            if (seenOwnNotarisations == 1)
                moms.push_back(data.MoM);
        }
    }

end:
    return GetMerkleRoot(moms);
}


/* On KMD */
TxProof GetCrossChainProof(const uint256 txid, const char* targetSymbol, uint32_t targetCCid,
        const TxProof assetChainProof)
{
    /*
     * Here we are given a proof generated by an assetchain A which goes from given txid to
     * an assetchain MoM. We need to go from the notarisationTxid for A to the MoMoM range of the
     * backnotarisation for B (given by kmdheight of notarisation), find the MoM within the MoMs for
     * that range, and finally extend the proof to lead to the MoMoM (proof root).
     */
    EvalRef eval;
    uint256 MoM = assetChainProof.second.Exec(txid);
    
    // Get a kmd height for given notarisation Txid
    int kmdHeight;
    {
        CTransaction sourceNotarisation;
        uint256 hashBlock;
        CBlockIndex blockIdx;
        if (eval->GetTxConfirmed(assetChainProof.first, sourceNotarisation, blockIdx))
            kmdHeight = blockIdx.nHeight;
        else if (eval->GetTxUnconfirmed(assetChainProof.first, sourceNotarisation, hashBlock))
            kmdHeight = chainActive.Tip()->nHeight;
        else
            throw std::runtime_error("Notarisation not found");
    }

    // Get MoMs for kmd height and symbol
    std::vector<uint256> moms;
    uint256 targetChainNotarisationTxid;
    uint256 MoMoM = CalculateProofRoot(targetSymbol, targetCCid, kmdHeight, moms, targetChainNotarisationTxid);
    if (MoMoM.IsNull())
        throw std::runtime_error("No MoMs found");
    
    // Find index of source MoM in MoMoM
    int nIndex;
    for (nIndex=0; nIndex<moms.size(); nIndex++) {
        if (moms[nIndex] == MoM)
            goto cont;
    }
    throw std::runtime_error("Couldn't find MoM within MoMoM set");
cont:

    // Create a branch
    std::vector<uint256> vBranch;
    {
        CBlock fakeBlock;
        for (int i=0; i<moms.size(); i++) {
            CTransaction fakeTx;
            // first value in CTransaction memory is it's hash
            memcpy((void*)&fakeTx, moms[i].begin(), 32);
            fakeBlock.vtx.push_back(fakeTx);
        }
        vBranch = fakeBlock.GetMerkleBranch(nIndex);
    }

    // Concatenate branches
    MerkleBranch newBranch = assetChainProof.second;
    newBranch << MerkleBranch(nIndex, vBranch);

    // Check proof
    if (newBranch.Exec(txid) != MoMoM)
        throw std::runtime_error("Proof check failed");

    return std::make_pair(targetChainNotarisationTxid,newBranch);
}


/*
 * Takes an importTx that has proof leading to assetchain root
 * and extends proof to cross chain root
 */
void CompleteImportTransaction(CTransaction &importTx)
{
    TxProof proof;
    CTransaction burnTx;
    std::vector<CTxOut> payouts;
    if (!UnmarshalImportTx(importTx, proof, burnTx, payouts))
        throw std::runtime_error("Couldn't parse importTx");

    std::string targetSymbol;
    uint32_t targetCCid;
    uint256 payoutsHash;
    if (!UnmarshalBurnTx(burnTx, targetSymbol, &targetCCid, payoutsHash))
        throw std::runtime_error("Couldn't parse burnTx");

    proof = GetCrossChainProof(burnTx.GetHash(), targetSymbol.data(), targetCCid, proof);

    importTx = MakeImportCoinTransaction(proof, burnTx, importTx.vout);
}


struct notarized_checkpoint *komodo_npptr_at(int idx);
struct notarized_checkpoint *komodo_npptr_for_height(int32_t height, int *idx);


/* On assetchain */
bool GetNextBacknotarisation(uint256 kmdNotarisationTxid, std::pair<uint256,NotarisationData> &out)
{
    /*
     * Here we are given a txid, and a proof.
     * We go from the KMD notarisation txid to the backnotarisation,
     * then jump to the next backnotarisation, which contains the corresponding MoMoM.
     */
    Notarisation bn;
    if (!GetBackNotarisation(kmdNotarisationTxid, bn))
        return false;

    int npIdx;
    struct notarized_checkpoint* np = komodo_npptr_for_height(bn.second.height, &npIdx);
    if (!(np = komodo_npptr_at(npIdx+1)))
        return false;

    return GetBackNotarisation(np->notarized_desttxid, out);
        throw std::runtime_error("Can't get backnotarisation");
}


struct notarized_checkpoint* komodo_npptr(int32_t height);

int32_t komodo_MoM(int32_t *notarized_htp,uint256 *MoMp,uint256 *kmdtxidp,int32_t nHeight,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip);

/*
 * On assetchain
 * in: txid
 * out: pair<notarisationTxHash,merkleBranch>
 */
TxProof GetAssetchainProof(uint256 hash)
{
    int nIndex;
    CBlockIndex* blockIndex;
    struct notarized_checkpoint* np;
    std::vector<uint256> branch;

    {
        uint256 blockHash;
        CTransaction tx;
        if (!GetTransaction(hash, tx, blockHash, true))
            throw std::runtime_error("cannot find transaction");

        blockIndex = mapBlockIndex[blockHash];
        if (!(np = komodo_npptr(blockIndex->nHeight)))
            throw std::runtime_error("notarisation not found");
        
        // index of block in MoM leaves
        nIndex = np->notarized_height - blockIndex->nHeight;
    }

    // build merkle chain from blocks to MoM
    {
        // since the merkle branch code is tied up in a block class
        // and we want to make a merkle branch for something that isnt transactions
        CBlock fakeBlock;
        for (int i=0; i<np->MoMdepth; i++) {
            uint256 mRoot = chainActive[np->notarized_height - i]->hashMerkleRoot;
            CTransaction fakeTx;
            // first value in CTransaction memory is it's hash
            memcpy((void*)&fakeTx, mRoot.begin(), 32);
            fakeBlock.vtx.push_back(fakeTx);
        }
        branch = fakeBlock.GetMerkleBranch(nIndex);

        // Check branch
        if (np->MoM != CBlock::CheckMerkleBranch(blockIndex->hashMerkleRoot, branch, nIndex))
            throw std::runtime_error("Failed merkle block->MoM");
    }

    // Now get the tx merkle branch
    {
        CBlock block;

        if (fHavePruned && !(blockIndex->nStatus & BLOCK_HAVE_DATA) && blockIndex->nTx > 0)
            throw std::runtime_error("Block not available (pruned data)");

        if(!ReadBlockFromDisk(block, blockIndex,1))
            throw std::runtime_error("Can't read block from disk");

        // Locate the transaction in the block
        int nTxIndex;
        for (nTxIndex = 0; nTxIndex < (int)block.vtx.size(); nTxIndex++)
            if (block.vtx[nTxIndex].GetHash() == hash)
                break;

        if (nTxIndex == (int)block.vtx.size())
            throw std::runtime_error("Error locating tx in block");

        std::vector<uint256> txBranch = block.GetMerkleBranch(nTxIndex);

        // Check branch
        if (block.hashMerkleRoot != CBlock::CheckMerkleBranch(hash, txBranch, nTxIndex))
            throw std::runtime_error("Failed merkle tx->block");

        // concatenate branches
        nIndex = (nIndex << txBranch.size()) + nTxIndex;
        branch.insert(branch.begin(), txBranch.begin(), txBranch.end());
    }

    // Check the proof
    if (np->MoM != CBlock::CheckMerkleBranch(hash, branch, nIndex)) 
        throw std::runtime_error("Failed validating MoM");

    // All done!
    CDataStream ssProof(SER_NETWORK, PROTOCOL_VERSION);
    return std::make_pair(np->notarized_desttxid, MerkleBranch(nIndex, branch));
}


