#ifndef __lms7002_pnlTxTSP_view__
#define __lms7002_pnlTxTSP_view__

/**
@file
Subclass of pnlTxTSP_view, which is generated by wxFormBuilder.
*/

#include "lms7002_wxgui.h"

//// end generated include
#include <map>
#include <vector>
#include "LMS7002M_parameters.h"
namespace lime{
class LMS7002M;
}
/** Implementing pnlTxTSP_view */
class lms7002_pnlTxTSP_view : public pnlTxTSP_view
{
	protected:
		// Handlers for pnlTxTSP_view events.
		void ParameterChangeHandler( wxSpinEvent& event );
		void ParameterChangeHandler( wxCommandEvent& event );
        void OnNCOSelectionChange(wxCommandEvent& event);
		void onbtnReadBISTSignature( wxCommandEvent& event );
		void OnbtnLoadDCIClick( wxCommandEvent& event );
		void OnbtnLoadDCQClick( wxCommandEvent& event );
		void onbtnGFIR1Coef( wxCommandEvent& event );
		void onbtnGFIR2Coef( wxCommandEvent& event );
		void onbtnGFIR3Coef( wxCommandEvent& event );
		void OnbtnUploadNCOClick( wxCommandEvent& event );
	public:
		/** Constructor */
		lms7002_pnlTxTSP_view( wxWindow* parent );
	//// end generated class members
	lms7002_pnlTxTSP_view(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL);
    void Initialize(lime::LMS7002M* pControl);
    void UpdateGUI();
    void UpdateNCOinputs();
protected:
    void PHOinputChanged(wxCommandEvent& event);
    lime::LMS7002M* lmsControl;
	std::map<wxWindow*, lime::LMS7Parameter> wndId2Enum;

    std::vector<wxStaticText*> lblNCOangles;
    std::vector<wxRadioButton*> rgrNCOselections;
    std::vector<wxTextCtrl*> txtNCOinputs;
};

#endif // __lms7002_pnlTxTSP_view__
