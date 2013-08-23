/*
Copyright (c) 2009-2013, UT-Battelle, LLC
All rights reserved

[DMRG++, Version 2.0.0]
[by G.A., Oak Ridge National Laboratory]

UT Battelle Open Source Software License 11242008

OPEN SOURCE LICENSE

Subject to the conditions of this License, each
contributor to this software hereby grants, free of
charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), a
perpetual, worldwide, non-exclusive, no-charge,
royalty-free, irrevocable copyright license to use, copy,
modify, merge, publish, distribute, and/or sublicense
copies of the Software.

1. Redistributions of Software must retain the above
copyright and license notices, this list of conditions,
and the following disclaimer.  Changes or modifications
to, or derivative works of, the Software should be noted
with comments and the contributor and organization's
name.

2. Neither the names of UT-Battelle, LLC or the
Department of Energy nor the names of the Software
contributors may be used to endorse or promote products
derived from this software without specific prior written
permission of UT-Battelle.

3. The software and the end-user documentation included
with the redistribution, with or without modification,
must include the following acknowledgment:

"This product includes software produced by UT-Battelle,
LLC under Contract No. DE-AC05-00OR22725  with the
Department of Energy."

*********************************************************
DISCLAIMER

THE SOFTWARE IS SUPPLIED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER, CONTRIBUTORS, UNITED STATES GOVERNMENT,
OR THE UNITED STATES DEPARTMENT OF ENERGY BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

NEITHER THE UNITED STATES GOVERNMENT, NOR THE UNITED
STATES DEPARTMENT OF ENERGY, NOR THE COPYRIGHT OWNER, NOR
ANY OF THEIR EMPLOYEES, REPRESENTS THAT THE USE OF ANY
INFORMATION, DATA, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED WOULD NOT INFRINGE PRIVATELY OWNED RIGHTS.

*********************************************************


*/
/** \ingroup DMRG */
/*@{*/

/*! \file ObserverInterpreter.h
 *
 * TBW
 *
 */

#ifndef OBSERVER_INTERPRETER_H
#define OBSERVER_INTERPRETER_H
#include <iostream>
#include "String.h"
#include "Vector.h"
#include "Tokenizer.h"

namespace Dmrg {

template<typename ObservableLibraryType>
class ObserverInterpreter {

	typedef typename ObservableLibraryType::ModelType ModelType;
	typedef PsimagLite::Vector<PsimagLite::String>::Type VectorStringType;
	typedef typename ObservableLibraryType::PreOperatorSiteIndependentType
	PreOperatorSiteIndependentType;
	typedef typename ObservableLibraryType::OperatorType OperatorType;
	typedef typename OperatorType::SparseMatrixType SparseMatrixType;
	typedef typename SparseMatrixType::value_type ComplexOrRealType;
	typedef typename PsimagLite::Vector<OperatorType>::Type VectorOperatorType;
	typedef PsimagLite::Matrix<ComplexOrRealType> MatrixType;

	enum {ONE_POINT, TWO_POINT};

	class Bracket {

	public:

		Bracket(const ModelType& model,const PsimagLite::String& bracket)
		    : model_(model), bracket_(2,""),type_(ONE_POINT)
		{
			VectorStringType vecStr;
			PsimagLite::tokenizer(bracket,vecStr,"|");

			if (vecStr.size() != 3) {
				PsimagLite::String str("ObserverInterpreter: syntax error for ");
				str += bracket + " is not a bracket\n";
				throw PsimagLite::RuntimeError(str);
			}

			bracket_[0] = vecStr[0].substr(1,vecStr[0].length()-1);
			if (!isBracket(0)) {
				PsimagLite::String str("ObserverInterpreter: syntax error: ");
				str += bracket_[0] + " must be <gs or <time \n";
				throw PsimagLite::RuntimeError(str);
			}

			bracket_[1] = vecStr[2].substr(0,vecStr[2].length()-1);
			if (!isBracket(1)) {
				PsimagLite::String str("ObserverInterpreter: syntax error: ");
				str += bracket_[1] + " must be <gs or <time \n";
				throw PsimagLite::RuntimeError(str);
			}

			PsimagLite::tokenizer(vecStr[1],name_,";");

			if (name_.size() != 1 && name_.size() != 2) {
				PsimagLite::String str("ObserverInterpreter: syntax error for ");
				str += bracket + " expected one or two operators\n";
				throw PsimagLite::RuntimeError(str);
			}

			op_.push_back(findOperator(name_[0]));

			if (name_.size() == 2) {
				type_ = TWO_POINT;
				op_.push_back(findOperator(name_[1]));
			} else {
				assert(vecStr.size() == 1);
			}
		}

		const OperatorType& op(SizeType ind) const
		{
			assert(ind < op_.size());
			return op_[ind];
		}

		PsimagLite::String opName(SizeType ind) const
		{
			assert(ind < name_.size());
			return name_[ind];
		}

		PsimagLite::String bra() const
		{
			assert(isBracket(0));
			return bracket_[0];
		}

		PsimagLite::String ket() const
		{
			assert(isBracket(1));
			return bracket_[1];
		}

		bool type() const { return type_; }


	private:

		bool isBracket(SizeType ind) const
		{
			if (ind >= bracket_.size()) return false;
			return (bracket_[ind] == "gs" || bracket_[ind] == "time");
		}

		OperatorType findOperator(const PsimagLite::String& name) const
		{
			if (name.length()<2 || name[0]!=':') {
				PsimagLite::String str("ObserverInterpreter: syntax error for ");
				str += name + "\n";
				throw PsimagLite::RuntimeError(str);
			}

			PsimagLite::String label = name.substr(1,name.length()-1);

			PsimagLite::IoSimple::In io(label);

			CookedOperator<ModelType> cookedOperator(model_);

			return OperatorType(io,cookedOperator,OperatorType::MUST_BE_NONZERO);
		}

		const ModelType& model_;
		VectorStringType bracket_;
		VectorStringType name_;
		SizeType type_;
		VectorOperatorType op_;
	}; // class Bracket

public:

	ObserverInterpreter(ObservableLibraryType& observableLibrary)
	    : observableLibrary_(observableLibrary)
	{}

	void operator()(const PsimagLite::String& list, SizeType rows, SizeType cols)
	{
		VectorStringType vecStr;
		PsimagLite::tokenizer(list,vecStr,",");

		for (SizeType i = 0; i < vecStr.size(); ++i) {
			std::cout<<vecStr[i]<<"\n";

			Bracket bracket(observableLibrary_.model(), vecStr[i]);

			SizeType threadId = 0;
			if (bracket.type() == ONE_POINT) {
				PreOperatorSiteIndependentType preOperator(bracket.op(0),
				                                           bracket.opName(0),
				                                           threadId);
				observableLibrary_.measureOnePoint(bracket.bra(),
				                                   preOperator,
				                                   bracket.ket());
			} else {
				MatrixType m0;
				crsMatrixToFullMatrix(m0,bracket.op(0).data);

				MatrixType m1;
				crsMatrixToFullMatrix(m1,bracket.op(1).data);

				observableLibrary_.measureOne(bracket.opName(0),
				                              m0,
				                              bracket.opName(1),
				                              m1,
				                              bracket.op(0).fermionSign,
				                              rows,
				                              cols,
				                              threadId);
			}
		}
	}

private:

	ObservableLibraryType& observableLibrary_;

}; //class ObserverInterpreter
} // namespace Dmrg

/*@}*/
#endif
