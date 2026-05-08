# Hierarchical structure test
# This test simulates a company organization structure with departments and employees.

create_employee id, name, role:
  return {
    id: id,
    name: name,
    role: role,
    projects: []
  }

create_department name:
  return {
    name: name,
    employees: [],
    sub_departments: []
  }

add_employee dept, emp:
  emps = dept.employees
  emps.push(emp)

add_sub_dept dept, sub:
  subs = dept.sub_departments
  subs.push(sub)

# Recursive function to print organization
print_org dept, indent:
  print(indent + "Department: " + toString(dept.name))
  for emp in dept.employees:
    print(indent + "  - Employee: " + toString(emp.name) + " (" + toString(emp.role) + ")")
  
  for sub in dept.sub_departments:
    print_org(sub, indent + "    ")

main:
  print("=== COMPANY ORG TEST ===")
  
  head_office = create_department("Head Office")
  
  # Tech Dept
  tech = create_department("Technology")
  add_employee(tech, create_employee(1, "Alice", "CTO"))
  add_employee(tech, create_employee(2, "Bob", "Lead Engineer"))
  
  # Backend Sub-Dept
  backend = create_department("Backend Team")
  add_employee(backend, create_employee(3, "Charlie", "Senior Dev"))
  add_employee(backend, create_employee(4, "David", "Junior Dev"))
  add_sub_dept(tech, backend)
  
  # Frontend Sub-Dept
  frontend = create_department("Frontend Team")
  add_employee(frontend, create_employee(5, "Eve", "UI Designer"))
  add_sub_dept(tech, frontend)
  
  # HR Dept
  hr = create_department("Human Resources")
  add_employee(hr, create_employee(6, "Frank", "HR Manager"))
  
  add_sub_dept(head_office, tech)
  add_sub_dept(head_office, hr)
  
  print_org(head_office, "")
  
  print("=== ORG TEST PASSED ===")
