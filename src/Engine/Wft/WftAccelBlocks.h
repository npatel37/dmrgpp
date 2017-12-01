#ifndef WFTACCELBLOCKS_H
#define WFTACCELBLOCKS_H
#include "Matrix.h"
#include "BLAS.h"

namespace Dmrg {

template<typename WaveFunctionTransfBaseType>
class WftAccelBlocks {

	typedef typename WaveFunctionTransfBaseType::DmrgWaveStructType DmrgWaveStructType;
	typedef typename WaveFunctionTransfBaseType::WftOptions WftOptionsType;
	typedef typename WaveFunctionTransfBaseType::VectorWithOffsetType VectorWithOffsetType;
	typedef typename WaveFunctionTransfBaseType::VectorSizeType VectorSizeType;
	typedef typename DmrgWaveStructType::LeftRightSuperType LeftRightSuperType;
	typedef typename VectorWithOffsetType::VectorType VectorType;
	typedef typename VectorType::value_type ComplexOrRealType;
	typedef typename DmrgWaveStructType::BasisWithOperatorsType BasisWithOperatorsType;
	typedef typename BasisWithOperatorsType::SparseMatrixType SparseMatrixType;
	typedef PsimagLite::Matrix<ComplexOrRealType> MatrixType;
	typedef typename PsimagLite::Vector<MatrixType>::Type VectorMatrixType;
	typedef typename WaveFunctionTransfBaseType::PackIndicesType PackIndicesType;

public:

	WftAccelBlocks(const DmrgWaveStructType& dmrgWaveStruct,
	               const WftOptionsType& wftOptions)
	    : dmrgWaveStruct_(dmrgWaveStruct), wftOptions_(wftOptions)
	{}

	void environFromInfinite(VectorWithOffsetType& psiDest,
	                         SizeType i0,
	                         const VectorWithOffsetType& psiSrc,
	                         SizeType i0src,
	                         const LeftRightSuperType& lrs,
	                         const VectorSizeType& nk) const
	{
		SizeType volumeOfNk = DmrgWaveStructType::volumeOf(nk);
		SizeType jp2size = dmrgWaveStruct_.lrs.super().size();
		SizeType i2psize = dmrgWaveStruct_.lrs.left().permutationInverse().size()/volumeOfNk;

		VectorMatrixType psi(volumeOfNk);
		for (SizeType kp = 0; kp < volumeOfNk; ++kp) {
			psi[kp].resize(i2psize, jp2size);
			psi[kp].setTo(0.0);
		}

		environPreparePsi(psi, psiSrc, i0src, volumeOfNk);
		MatrixType ws;
		dmrgWaveStruct_.ws.toDense(ws);

		MatrixType we;
		dmrgWaveStruct_.we.toDense(we);

		SizeType jpsize = we.cols();
		SizeType ipsize = ws.rows();
		MatrixType tmp(i2psize, jpsize);
		VectorMatrixType result(volumeOfNk);

		for (SizeType kp = 0; kp < volumeOfNk; ++kp) {
			result[kp].resize(ipsize, jpsize);
			result[kp].setTo(0.0);
			SizeType ip = result[kp].rows();
			tmp.setTo(0.0);

			psimag::BLAS::GEMM('N',
			                   'N',
			                   i2psize,
			                   jpsize,
			                   jp2size,
			                   1.0,
			                   &((psi[kp])(0,0)),
			                   i2psize,
			                   &(we(0,0)),
			                   jp2size,
			                   0.0,
			                   &(tmp(0,0)),
			                   i2psize);

			psimag::BLAS::GEMM('N',
			                   'N',
			                   ipsize,
			                   jpsize,
			                   i2psize,
			                   1.0,
			                   &(ws(0,0)),
			                   ip,
			                   &(tmp(0,0)),
			                   i2psize,
			                   0.0,
			                   &((result[kp])(0,0)),
			                   ipsize);
		}

		environCopyOut(psiDest, i0, result, lrs, volumeOfNk);
	}

	void systemFromInfinite(VectorWithOffsetType& psiDest,
	                        SizeType i0,
	                        const VectorWithOffsetType& psiSrc,
	                        SizeType i0src,
	                        const LeftRightSuperType& lrs,
	                        const VectorSizeType& nk) const
	{
		SizeType volumeOfNk = DmrgWaveStructType::volumeOf(nk);
		MatrixType ws;
		dmrgWaveStruct_.ws.toDense(ws);

		MatrixType we;
		dmrgWaveStruct_.we.toDense(we);

		SizeType isSize = ws.cols();
		SizeType ipSize = ws.rows();
		SizeType jprSize = we.cols();
		SizeType jenSize = we.rows();

		VectorMatrixType psi(volumeOfNk);
		for (SizeType kp = 0; kp < volumeOfNk; ++kp) {
			psi[kp].resize(ipSize, jprSize);
			psi[kp].setTo(0.0);
		}

		systemPreparePsi(psi, psiSrc, i0src, volumeOfNk);

		MatrixType tmp(ipSize, jenSize);
		VectorMatrixType result(volumeOfNk);

		for (SizeType jpl = 0; jpl < volumeOfNk; ++jpl) {
			result[jpl].resize(isSize, jenSize);
			result[jpl].setTo(0.0);
			tmp.setTo(0.0);

			psimag::BLAS::GEMM('N',
			                   'C',
			                   ipSize,
			                   jenSize,
			                   jprSize,
			                   1.0,
			                   &((psi[jpl])(0,0)),
			                   ipSize,
			                   &(we(0,0)),
			                   jprSize,
			                   0.0,
			                   &(tmp(0,0)),
			                   ipSize);

			psimag::BLAS::GEMM('C',
			                   'N',
			                   isSize,
			                   jenSize,
			                   ipSize,
			                   1.0,
			                   &(ws(0,0)),
			                   isSize,
			                   &(tmp(0,0)),
			                   ipSize,
			                   0.0,
			                   &((result[jpl])(0,0)),
			                   isSize);
		}

		systemCopyOut(psiDest, i0, result, lrs, volumeOfNk);
	}

private:

	void environPreparePsi(VectorMatrixType& psi,
	                       const VectorWithOffsetType& psiSrc,
	                       SizeType i0src,
	                       SizeType volumeOfNk) const
	{
		SizeType total = psiSrc.effectiveSize(i0src);
		SizeType offset = psiSrc.offset(i0src);
		PackIndicesType packSuper(dmrgWaveStruct_.lrs.left().size());
		PackIndicesType packLeft(dmrgWaveStruct_.lrs.left().size()/volumeOfNk);

		for (SizeType x = 0; x < total; ++x) {
			SizeType alpha = 0;
			SizeType jp2 = 0;
			packSuper.unpack(alpha, jp2, dmrgWaveStruct_.lrs.super().permutation(x + offset));
			SizeType ip2 = 0;
			SizeType kp = 0;
			packLeft.unpack(ip2, kp, dmrgWaveStruct_.lrs.left().permutation(alpha));
			psi[kp](ip2, jp2) += psiSrc.fastAccess(i0src, x);
		}
	}

	void environCopyOut(VectorWithOffsetType& psiDest,
	                    SizeType i0,
	                    const VectorMatrixType& result,
	                    const LeftRightSuperType& lrs,
	                    SizeType volumeOfNk) const
	{
		SizeType nip = lrs.super().permutationInverse().size()/
		        lrs.right().permutationInverse().size();
		PackIndicesType pack1(nip);
		PackIndicesType pack2(volumeOfNk);
		SizeType total = psiDest.effectiveSize(i0);
		SizeType start = psiDest.offset(i0);

		for (SizeType x = 0; x < total; ++x) {
			SizeType ip = 0;
			SizeType beta = 0;
			pack1.unpack(ip,beta,(SizeType)lrs.super().permutation(x+start));
			SizeType kp = 0;
			SizeType jp = 0;
			pack2.unpack(kp,jp,(SizeType)lrs.right().permutation(beta));
			psiDest.fastAccess(i0, x) += result[kp](ip, jp);
		}
	}

	void systemPreparePsi(VectorMatrixType& psi,
	                      const VectorWithOffsetType& psiSrc,
	                      SizeType i0src,
	                      SizeType volumeOfNk) const
	{
		SizeType total = psiSrc.effectiveSize(i0src);
		SizeType offset = psiSrc.offset(i0src);
		PackIndicesType packSuper(dmrgWaveStruct_.lrs.left().size());
		PackIndicesType packRight(volumeOfNk);

		for (SizeType y = 0; y < total; ++y) {
			SizeType ip = 0;
			SizeType jp = 0;
			packSuper.unpack(ip, jp, dmrgWaveStruct_.lrs.super().permutation(y + offset));
			SizeType jpl = 0;
			SizeType jpr = 0;
			packRight.unpack(jpl, jpr, dmrgWaveStruct_.lrs.right().permutation(jp));
			psi[jpl](ip, jpr) = psiSrc.fastAccess(i0src, y);
		}
	}

	void systemCopyOut(VectorWithOffsetType& psiDest,
	                   SizeType i0,
	                   const VectorMatrixType& result,
	                   const LeftRightSuperType& lrs,
	                   SizeType volumeOfNk) const
	{
		SizeType nip = lrs.left().permutationInverse().size()/volumeOfNk;
		SizeType nalpha = lrs.left().permutationInverse().size();
		PackIndicesType pack1(nalpha);
		PackIndicesType pack2(nip);
		SizeType total = psiDest.effectiveSize(i0);
		SizeType start = psiDest.offset(i0);

		for (SizeType x = 0; x < total; ++x) {
			SizeType isn = 0;
			SizeType jen = 0;
			pack1.unpack(isn, jen, lrs.super().permutation(x+start));
			SizeType is = 0;
			SizeType jpl = 0;
			pack2.unpack(is, jpl, lrs.left().permutation(isn));
			psiDest.fastAccess(i0, x) += result[jpl](is, jen);
		}
	}

	const DmrgWaveStructType& dmrgWaveStruct_;
	const WftOptionsType& wftOptions_;
};
}
#endif // WFTACCELBLOCKS_H
