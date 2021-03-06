#include "ObserveDriver.h"

using namespace Dmrg;

template<typename GeometryType,
         typename ModelHelperType,
         typename VectorWithOffsetType>
void mainLoop(GeometryType& geometry,
              InputNgType::Readable& io,
              const ParametersDmrgSolverType& params,
              const PsimagLite::String& list)
{
	typedef typename VectorWithOffsetType::value_type ComplexOrRealType;

	typedef ModelBase<ModelHelperType,
	        ParametersDmrgSolverType,
	        InputNgType::Readable,
	        GeometryType> ModelBaseType;

	SizeType orbitals = 1.0;
	try {
		io.readline(orbitals,"Orbitals=");
	} catch (std::exception&) {}

	ModelSelector<ModelBaseType> modelSelector(params.model);
	const ModelBaseType& model = modelSelector(params,io,geometry);

	const PsimagLite::String& datafile = params.filename;
	IoInputType dataIo(datafile);

	bool iscomplex = false;
	dataIo.read(iscomplex, "IsComplex");

	if (iscomplex != PsimagLite::IsComplexNumber<ComplexOrRealType>::True)
		err("Previous run was complex and this one is not (or viceversa)\n");

	while (!observeOneFullSweep<VectorWithOffsetType,ModelBaseType>
	       (dataIo, model, list, orbitals));
}

template<typename GeometryType,
         template<typename> class ModelHelperTemplate,
         typename MySparseMatrix>
void mainLoop1(GeometryType& geometry,
               InputNgType::Readable& io,
               const ParametersDmrgSolverType& params,
               const PsimagLite::String& list)
{
	typedef Basis<MySparseMatrix> BasisType;
	typedef Operators<BasisType> OperatorsType;
	typedef BasisWithOperators<OperatorsType> BasisWithOperatorsType;
	typedef LeftRightSuper<BasisWithOperatorsType,BasisType> LeftRightSuperType;
	typedef ModelHelperTemplate<LeftRightSuperType> ModelHelperType;
	typedef typename MySparseMatrix::value_type ComplexOrRealType;
	typedef Qn QnType;

	if (params.options.find("vectorwithoffsets")!=PsimagLite::String::npos) {
		typedef VectorWithOffsets<ComplexOrRealType, QnType> VectorWithOffsetType;
		mainLoop<GeometryType,ModelHelperType,VectorWithOffsetType>
		        (geometry, io, params, list);
	} else {
		typedef VectorWithOffset<ComplexOrRealType, QnType> VectorWithOffsetType;
		mainLoop<GeometryType,ModelHelperType,VectorWithOffsetType>
		        (geometry, io, params, list);
	}
}

template<typename MySparseMatrix>
void mainLoop0(InputNgType::Readable& io,
               ParametersDmrgSolverType& dmrgSolverParams,
               InputCheck& inputCheck,
               const PsimagLite::String& list)
{
	typedef typename MySparseMatrix::value_type ComplexOrRealType;
	typedef PsimagLite::Geometry<ComplexOrRealType,
	        InputNgType::Readable,
	        ProgramGlobals> GeometryType;

	GeometryType geometry(io);
	int tmp = 0;
	try {
		io.readline(tmp,"UseSu2Symmetry=");
	} catch (std::exception&) {}

	bool su2 = (tmp > 0);

	if (su2) {
		mainLoop1<GeometryType, ModelHelperSu2, MySparseMatrix>
		        (geometry, io, dmrgSolverParams, list);
	} else {
		mainLoop1<GeometryType, ModelHelperLocal, MySparseMatrix>
		        (geometry, io, dmrgSolverParams, list);
	}
}

void usage(const PsimagLite::String& name)
{
	std::cerr<<"USAGE is "<<name<<" -f filename [-p precision] [-F fileoption]";
	std::cerr<<" [-V] whatToMeasure\n";
}

/* PSIDOC ObserveDriver
 The observe driver may be used to measure after the main dmrg run has finished.
The observe driver is used as follows,
\begin{verbatim}
./observe -f input.inp whatToMeasure
\end{verbatim}
  The command line arguments of observe are the following.
\begin{itemize}
\item[-f] [Mandatory, String] Input to use. Files
 referred to by \verb!OutputFile=! are now inputs, and
must be present.
\item[whatToMeasure] {[}Mandatory, String{]} What to measure post process.
This is a comma-separated list of braket specifications.
Braket specifications can be bare or dressed, and are explained elsewhere.
\item[-p] [Optional, Integer] Digits of precision for printing.
\item[-o] {[}Optional, String{]} Extra options for SolverOptions
\item[-F] [Optional, string] TBW
\item[-S] [Optional] Ignore the Threads= line if present in the input, and run with
Threads=1
\item[-V] [Optional] Print version and exit
\end{itemize}
  */
int main(int argc,char **argv)
{
	using namespace Dmrg;
	PsimagLite::PsiApp application("observe",&argc,&argv,1);
	PsimagLite::String filename;
	PsimagLite::String filesOption;
	int opt = 0;
	int precision = 6;
	bool forceSerial = false;
	bool versionOnly = false;
	PsimagLite::String sOptions("");

	while ((opt = getopt(argc, argv,"f:p:o:F:SV")) != -1) {
		switch (opt) {
		case 'f':
			filename = optarg;
			break;
		case 'p':
			precision = atoi(optarg);
			std::cout.precision(precision);
			std::cerr.precision(precision);
			break;
		case 'o':
			sOptions += optarg;
			break;
		case 'F':
			filesOption = optarg;
			break;
		case 'S':
			forceSerial = true;
			break;
		case 'V':
			versionOnly = true;
			break;
		default:
			usage(application.name());
			return 1;
		}
	}

	PsimagLite::String list = (optind < argc) ? argv[optind] : "";

	//sanity checks here
	if (filename=="" || (filesOption != "keep" && filesOption != "")) {
		if (!versionOnly) {
			usage(application.name());
			return 1;
		}
	}

	typedef PsimagLite::Concurrency ConcurrencyType;

	// print license
	if (ConcurrencyType::root()) {
		Provenance provenance;
		std::cout<<provenance;
		std::cout<<Provenance::logo(application.name())<<"\n";
		application.checkMicroArch(std::cout, Provenance::compiledMicroArch());
	}

	if (versionOnly) return 0;

	application.printCmdLine(std::cout);

	//Setup the Geometry
	InputCheck inputCheck;
	InputFromDataOrNot<InputCheck> inputFromDataOrNot(filename, inputCheck);
	InputNgType::Readable io(inputFromDataOrNot.ioWriteable());

	ParametersDmrgSolverType dmrgSolverParams(io,sOptions,false,true);

	if (forceSerial) dmrgSolverParams.nthreads = 1;

	bool setAffinities = (dmrgSolverParams.options.find("setAffinities")
	                      != PsimagLite::String::npos);
	PsimagLite::CodeSectionParams codeSectionParams(dmrgSolverParams.nthreads, setAffinities);
	ConcurrencyType::setOptions(codeSectionParams);

	bool isComplex = (dmrgSolverParams.options.find("useComplex") != PsimagLite::String::npos);
	if (dmrgSolverParams.options.find("TimeStepTargeting") != PsimagLite::String::npos)
		isComplex = true;

	if (isComplex) {
		mainLoop0<MySparseMatrixComplex>(io,dmrgSolverParams,inputCheck, list);
	} else {
		mainLoop0<MySparseMatrixReal>(io,dmrgSolverParams,inputCheck, list);
	}

} // main

